/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * What every plugin needs to draw widgets on the display of a device (the aLabs HMI widgets extension,
 * hmi/alabs-hmi-widgets.h), kept in one place:
 *
 *  - which widgets are on the screen now (the host shows and hides them from another thread: `WidgetHost` keeps the
 *    showings so that run() can read them without a lock);
 *  - when to draw: run() gives `widgets_run` a number for the state each widget would draw (its `signature`), and a
 *    widget is drawn again only when the number changed or it was just shown, at most `WIDGET_REFRESH_HZ` times a second.
 *    It draws the whole area, which is at most 1 KB: the host merges what is pending before it reaches the display.
 *
 * The extension is optional. Where the host does not give it (stock MOD firmware, the MOD Duo, any other host) `draw` is
 * NULL and nothing here does anything.
 *
 * Threading (see the header of the extension): widget_shown and widget_hidden come from one non real-time thread, possibly
 * while run() is executing. A slot is written between two increments of its `sequence` (odd while it is being written), and
 * run() copies it and checks the sequence did not move: it never draws on a handle it could not read whole, and never on one
 * that was hidden before it started.
 */

#ifndef HMI_WIDGET_HOST_H
#define HMI_WIDGET_HOST_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "alabs-hmi-widgets.h"
#include "widget-canvas.h"

/** The same widget can be on several areas (two displays of the Duo X, a column and a footer): this many at once, in all */
#define WIDGET_MAX_SHOWINGS 6
/** A widget changes its picture at most this many times a second (a display cannot follow more, and the host queues each one) */
#define WIDGET_REFRESH_HZ 20

typedef struct {
    /** Odd while the slot is being written */
    uint32_t sequence;
    ALABS_HMI_Widget handle;
    int      widget;
    uint16_t width;
    uint16_t height;
} WidgetSlot;

typedef struct {
    /** The feature of the host: NULL when it has none */
    const ALABS_HMI_WidgetDraw* draw;

    /** The symbols of the widgets of the plugin, in the order of the plugin's own enum */
    const char* const* symbols;
    int                symbol_count;

    WidgetSlot slots[WIDGET_MAX_SHOWINGS];

    // Used by run() only
    uint32_t drawn_sequence[WIDGET_MAX_SHOWINGS];
    uint32_t drawn_signature[WIDGET_MAX_SHOWINGS];
    double   seconds_since_draw;
} WidgetHost;

/** The feature of the host in the `features` of instantiate(), or NULL */
static inline const ALABS_HMI_WidgetDraw* widgets_find_feature(const LV2_Feature* const* features) {
    for (int i = 0; features != NULL && features[i] != NULL; ++i) {
        if (strcmp(features[i]->URI, ALABS_HMI__WidgetDraw) == 0) {
            return (const ALABS_HMI_WidgetDraw*) features[i]->data;
        }
    }

    return NULL;
}

/** Call from instantiate() with the feature found in `features` (NULL when the host has none, or an old one) */
static inline void widgets_init(WidgetHost* host, const ALABS_HMI_WidgetDraw* draw, const char* const* symbols, int symbol_count) {
    memset(host, 0, sizeof(*host));

    host->draw = (draw != NULL && draw->size >= ALABS_HMI_WIDGETDRAW_SIZE_BASE && draw->draw_bitmap != NULL) ? draw : NULL;
    host->symbols = symbols;
    host->symbol_count = symbol_count;
    host->seconds_since_draw = 1.0;
}

/** True when some widget of the plugin is on a display now: the plugin may skip work that only the widgets need */
static inline bool widgets_visible(const WidgetHost* host) {
    for (int i = 0; i < WIDGET_MAX_SHOWINGS; ++i) {
        if (__atomic_load_n(&host->slots[i].handle, __ATOMIC_ACQUIRE) != NULL) {
            return true;
        }
    }

    return false;
}

// -------------------------------------------------------------------------------------------------------------------
// WidgetNotification: not the real-time thread

static inline void widgets_shown(WidgetHost* host, const char* symbol, ALABS_HMI_Widget handle, const ALABS_HMI_WidgetArea* area) {
    int widget = -1;

    for (int i = 0; i < host->symbol_count; ++i) {
        if (strcmp(host->symbols[i], symbol) == 0) {
            widget = i;
            break;
        }
    }

    if (widget < 0 || handle == NULL || area == NULL) {
        return;
    }

    for (int i = 0; i < WIDGET_MAX_SHOWINGS; ++i) {
        WidgetSlot* slot = &host->slots[i];

        if (__atomic_load_n(&slot->handle, __ATOMIC_ACQUIRE) != NULL) {
            continue;
        }

        const uint32_t sequence = slot->sequence;

        __atomic_store_n(&slot->sequence, sequence + 1, __ATOMIC_RELEASE);
        slot->widget = widget;
        slot->width = area->width;
        slot->height = area->height;
        __atomic_store_n(&slot->handle, handle, __ATOMIC_RELEASE);
        __atomic_store_n(&slot->sequence, sequence + 2, __ATOMIC_RELEASE);
        return;
    }

    // No room: the widget stays empty (the host shows its placeholder). Six areas at once is more than any device has
}

static inline void widgets_hidden(WidgetHost* host, ALABS_HMI_Widget handle) {
    for (int i = 0; i < WIDGET_MAX_SHOWINGS; ++i) {
        WidgetSlot* slot = &host->slots[i];

        if (__atomic_load_n(&slot->handle, __ATOMIC_ACQUIRE) != handle || handle == NULL) {
            continue;
        }

        const uint32_t sequence = slot->sequence;

        __atomic_store_n(&slot->sequence, sequence + 1, __ATOMIC_RELEASE);
        __atomic_store_n(&slot->handle, NULL, __ATOMIC_RELEASE);
        __atomic_store_n(&slot->sequence, sequence + 2, __ATOMIC_RELEASE);
    }
}

// -------------------------------------------------------------------------------------------------------------------
// run()

/** Draws the widget in the canvas, which has the size of its area: the plugin's own function, given its own state */
typedef void (*WidgetRender)(const void* plugin, int widget, Canvas* canvas);

/**
 * Call once per cycle, after the plugin has updated its state.
 *
 * @param signatures the number of each widget (indexed like `symbols`): it must change whenever what the widget draws does
 *                   (fold in the state it shows, and `canvas`-time things such as a blinking playhead, not its size)
 */
static inline void widgets_run(WidgetHost* host, uint32_t frames, double sample_rate, const uint32_t* signatures,
                               WidgetRender render, const void* plugin) {
    if (host->draw == NULL) {
        return;
    }

    host->seconds_since_draw += (double) frames / sample_rate;
    const bool may_refresh = host->seconds_since_draw >= 1.0 / WIDGET_REFRESH_HZ;
    bool drew = false;

    for (int i = 0; i < WIDGET_MAX_SHOWINGS; ++i) {
        const WidgetSlot* source = &host->slots[i];
        const uint32_t before = __atomic_load_n(&source->sequence, __ATOMIC_ACQUIRE);

        if ((before & 1u) != 0) {
            continue;
        }

        const ALABS_HMI_Widget handle = __atomic_load_n(&source->handle, __ATOMIC_ACQUIRE);
        const int      widget = source->widget;
        const uint16_t width = source->width;
        const uint16_t height = source->height;

        if (__atomic_load_n(&source->sequence, __ATOMIC_ACQUIRE) != before) {
            continue;
        }

        if (handle == NULL || widget < 0 || widget >= host->symbol_count) {
            // Nothing there: the next showing is a new one and is drawn whole
            host->drawn_sequence[i] = 0;
            continue;
        }

        const bool fresh = host->drawn_sequence[i] != before;

        if (!fresh && (!may_refresh || host->drawn_signature[i] == signatures[widget])) {
            continue;
        }

        Canvas canvas;
        canvas_init(&canvas, width, height);
        render(plugin, widget, &canvas);

        host->draw->draw_bitmap(host->draw->handle, handle, 0, 0, canvas.width, canvas.height, canvas.bits);
        host->drawn_sequence[i] = before;
        host->drawn_signature[i] = signatures[widget];
        drew = true;
    }

    if (drew) {
        host->seconds_since_draw = 0.0;
    }
}

/** Mix a value into a signature (FNV-1a over the bytes of a 32 bit value) */
static inline uint32_t widget_hash(uint32_t signature, uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        signature ^= (value >> (8 * i)) & 0xFFu;
        signature *= 16777619u;
    }

    return signature;
}

#define WIDGET_HASH_SEED 2166136261u

#endif
