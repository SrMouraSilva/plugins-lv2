/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The display widgets of Gossiper: `footswitches` (which of the seven are on, with the name of the last one pressed) and
 * `potentiometers` (the four, as a percentage). They show what the controllers do on any area of the display, with no
 * `Assign to notify` needed (the pop-ups need one, and are gone after a moment). See hmi/widget-host.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../hmi/widget-host.h"
#include "lv2-gossiper.h"
#include "lv2-gossiper-widgets.h"

static const char* const WIDGET_SYMBOLS[WIDGET_COUNT] = { "footswitches", "potentiometers" };

#define LABEL_MAX 16

typedef struct {
    bool on[TOTAL_GOSSIPER_FOOTSWITCHES];
    /** The footswitch that changed last (-1: none yet) */
    int  last;
    char labels[TOTAL_GOSSIPER_FOOTSWITCHES][LABEL_MAX];
    int  percent[TOTAL_GOSSIPER_POTENTIOMETER];
    /** Bit n: footswitch n+1 is addressed to a control of the device (0: none is known to be) */
    uint32_t addressed;
} View;

typedef struct {
    WidgetHost widgets;
    double     sample_rate;
    View       view;
    /** The footswitches addressed to the device, as the host told us (from another thread than run()): bit n is footswitch n+1 */
    uint32_t   addressed;
} Widgets;

void LV2_Gossiper_Widgets_initialize(Gossiper* self, const LV2_Feature* const* features, double sample_rate) {
    Widgets* widgets = (Widgets*) calloc(1, sizeof(Widgets));

    self->lv2->widgets = widgets;

    if (widgets == NULL) {
        return;
    }

    widgets->sample_rate = sample_rate;
    widgets->view.last = -1;
    widgets_init(&widgets->widgets, widgets_find_feature(features), WIDGET_SYMBOLS, WIDGET_COUNT);
}

void LV2_Gossiper_Widgets_set_addressed(Gossiper* self, unsigned int footswitch, bool addressed) {
    Widgets* widgets = (Widgets*) self->lv2->widgets;

    if (widgets == NULL || footswitch >= TOTAL_GOSSIPER_FOOTSWITCHES) {
        return;
    }

    if (addressed) {
        __atomic_fetch_or(&widgets->addressed, 1u << footswitch, __ATOMIC_RELEASE);
    } else {
        __atomic_fetch_and(&widgets->addressed, ~(1u << footswitch), __ATOMIC_RELEASE);
    }
}

void LV2_Gossiper_Widgets_cleanup(Gossiper* self) {
    free(self->lv2->widgets);
    self->lv2->widgets = NULL;
}

// ---------------------------------------------------------------------------------------------------------------------
// Drawing

/** A footswitch as a box with its number: filled when it is on */
static void draw_switch_box(Canvas* canvas, int x, int y, int width, int height, int number, bool on) {
    char text[4];

    snprintf(text, sizeof(text), "%d", number);

    if (on) {
        canvas_fill(canvas, x, y, width, height, true);
    } else {
        canvas_frame(canvas, x, y, width, height);
    }

    if (width >= 5 && height >= 7) {
        canvas_text_centred(canvas, x, x + width, y + (height - 5) / 2, FONT_STYLE_SMALL, text, !on);
    }
}

/** The name of the footswitch that changed last, with whether it was turned on or off */
static void draw_last(Canvas* canvas, const View* view, int y, bool allow_big) {
    const int w = canvas->width;

    if (view->last < 0) {
        canvas_text_fit(canvas, 0, y, w, false, "NO CHANGE YET", true);
        return;
    }

    const char* state = view->on[view->last] ? "ON" : "OFF";
    const int state_width = canvas_text_width(FONT_STYLE_SMALL, (int) strlen(state));

    canvas_text_fit(canvas, 0, y, w - state_width - 3, allow_big, view->labels[view->last], true);
    canvas_text_right(canvas, w, y + (allow_big ? 1 : 0), FONT_STYLE_SMALL, state, true);
}

/**
 * The footswitches an area shows, whatever its size: the ones addressed to a control of the device when the host told us
 * of any (the others are not in use), all of them otherwise.
 * @return how many, their indices in `shown`
 */
static int footswitches_shown(const View* view, int* shown) {
    int count = 0;

    for (int i = 0; i < TOTAL_GOSSIPER_FOOTSWITCHES; ++i) {
        if (view->addressed == 0 || ((view->addressed >> i) & 1u)) {
            shown[count++] = i;
        }
    }

    return count;
}

static void render_footswitches(const View* view, Canvas* canvas) {
    const int w = canvas->width;
    const int h = canvas->height;
    int shown[TOTAL_GOSSIPER_FOOTSWITCHES];
    const int count = footswitches_shown(view, shown);

    // A list: a row for each footswitch in use, with its name
    if (w >= 90 && h >= 9 * count) {
        const int row = h / count > 14 ? 14 : h / count;

        for (int k = 0; k < count; ++k) {
            const int i = shown[k];
            const int y = (h - row * count) / 2 + k * row;
            const int text_y = y + (row - 7) / 2;

            draw_switch_box(canvas, 0, y + 1, row - 1, row - 2, i + 1, view->on[i]);
            canvas_text_fit(canvas, row + 3, text_y, w - row - 3 - 18, true, view->labels[i], true);
            canvas_text_right(canvas, w, y + (row - 5) / 2, FONT_STYLE_SMALL, view->on[i] ? "ON" : "OFF", true);
        }
        return;
    }

    // A footer, or a row of a knob: a box for each footswitch
    if (h < 22) {
        const int box = h > 11 ? 11 : h;

        for (int k = 0; k < count; ++k) {
            const int left = k * w / count;
            draw_switch_box(canvas, left, (h - box) / 2, (k + 1) * w / count - left - 1, box, shown[k] + 1, view->on[shown[k]]);
        }
        return;
    }

    // Boxes in rows, then the name of the last one
    const int per_row = w >= 80 ? count : (count < 4 ? count : 4);
    const int rows = (count + per_row - 1) / per_row;
    const int box_h = h >= 26 ? 11 : 9;

    for (int k = 0; k < count; ++k) {
        const int column = k % per_row;
        const int row = k / per_row;
        const int left = column * w / per_row;
        draw_switch_box(canvas, left, row * (box_h + 2), (column + 1) * w / per_row - left - 1, box_h, shown[k] + 1, view->on[shown[k]]);
    }

    const int y = rows * (box_h + 2) + 2;
    if (h >= y + 7) {
        draw_last(canvas, view, y, true);
    }
}

static void render_potentiometers(const View* view, Canvas* canvas) {
    const int w = canvas->width;
    const int h = canvas->height;
    const int count = TOTAL_GOSSIPER_POTENTIOMETER;
    char text[8];

    // Wide enough and not too short: a row for each, "P1 [=====    ] 42%"
    if (w >= 60 && h >= 4 * 8) {
        const int row = h / count;

        for (int i = 0; i < count; ++i) {
            const int y = (h - row * count) / 2 + i * row;
            // The big font where the row is tall enough for it
            const FontStyle style = row >= 11 ? FONT_STYLE_BIG : FONT_STYLE_SMALL;
            const int line_y = y + (row - canvas_font_height(style)) / 2;

            snprintf(text, sizeof(text), "P%d", i + 1);
            canvas_text(canvas, 0, line_y, style, text, true);

            snprintf(text, sizeof(text), "%d%%", view->percent[i]);
            const int value_width = canvas_text_width(style, 4);
            canvas_text_right(canvas, w, line_y, style, text, true);

            const int bar_x = canvas_text_width(style, 2) + 3;
            const int bar_w = w - bar_x - value_width - 3;
            const int bar_h = row >= 9 ? 7 : 5;
            const int bar_y = y + (row - bar_h) / 2;

            canvas_frame(canvas, bar_x, bar_y, bar_w, bar_h);
            canvas_fill(canvas, bar_x + 1, bar_y + 1, (bar_w - 2) * view->percent[i] / 100, bar_h - 2, true);
        }
        return;
    }

    // A footer: four small bars side by side
    if (h < 14) {
        for (int i = 0; i < count; ++i) {
            const int left = i * w / count;
            const int cell = (i + 1) * w / count - left - 2;
            const int bar_h = h >= 9 ? 5 : 3;
            const int y = (h - bar_h) / 2;

            canvas_frame(canvas, left, y, cell, bar_h);
            canvas_fill(canvas, left + 1, y + 1, (cell - 2) * view->percent[i] / 100, bar_h - 2, true);
        }
        return;
    }

    // A column: four upright bars with their number under them
    const int bar_w = w / count - 2;
    const int top = 0;
    const int bottom = h - 7;

    for (int i = 0; i < count; ++i) {
        const int left = i * w / count + 1;
        const int bar_h = bottom - top;
        const int fill = (bar_h - 2) * view->percent[i] / 100;

        canvas_frame(canvas, left, top, bar_w, bar_h);
        canvas_fill(canvas, left + 1, bottom - 1 - fill, bar_w - 2, fill, true);

        snprintf(text, sizeof(text), "%d", i + 1);
        canvas_text_centred(canvas, left, left + bar_w, h - 5, FONT_STYLE_SMALL, text, true);
    }
}

static void render_widget(const void* plugin, int widget, Canvas* canvas) {
    const Widgets* widgets = (const Widgets*) ((const Gossiper*) plugin)->lv2->widgets;

    if (widget == WIDGET_FOOTSWITCHES) {
        render_footswitches(&widgets->view, canvas);
    } else if (widget == WIDGET_POTENTIOMETERS) {
        render_potentiometers(&widgets->view, canvas);
    }
}

void LV2_Gossiper_Widgets_run(Gossiper* self, uint32_t n_samples) {
    Widgets* widgets = (Widgets*) self->lv2->widgets;

    if (widgets == NULL || widgets->widgets.draw == NULL) {
        return;
    }

    View* view = &widgets->view;
    uint32_t switches_signature = WIDGET_HASH_SEED;
    uint32_t potentiometers_signature = WIDGET_HASH_SEED;

    view->addressed = __atomic_load_n(&widgets->addressed, __ATOMIC_ACQUIRE);

    for (int i = 0; i < TOTAL_GOSSIPER_FOOTSWITCHES; ++i) {
        const bool on = self->switches[i].activated;

        // The last one that changed, among those a small area shows
        if ((self->switches[i].updated || on != view->on[i]) && (view->addressed == 0 || ((view->addressed >> i) & 1u))) {
            view->last = i;
        }
        view->on[i] = on;

        const char* label = Gossiper_get_footswitch_label(self, (unsigned int) i);
        snprintf(view->labels[i], sizeof(view->labels[i]), "%s", label != NULL ? label : "");

        switches_signature = widget_hash(switches_signature, (uint32_t) on);
        for (const char* c = view->labels[i]; *c != '\0'; ++c) {
            switches_signature = widget_hash(switches_signature, (uint32_t) *c);
        }
    }
    switches_signature = widget_hash(switches_signature, (uint32_t) view->last);
    switches_signature = widget_hash(switches_signature, view->addressed);

    for (int i = 0; i < TOTAL_GOSSIPER_POTENTIOMETER; ++i) {
        float value = self->potentiometers[i].value;

        value = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
        view->percent[i] = (int) (value * 100.0f + 0.5f);
        potentiometers_signature = widget_hash(potentiometers_signature, (uint32_t) view->percent[i]);
    }

    const uint32_t signatures[WIDGET_COUNT] = { switches_signature, potentiometers_signature };
    widgets_run(&widgets->widgets, n_samples, widgets->sample_rate, signatures, render_widget, self);
}

// ---------------------------------------------------------------------------------------------------------------------
// WidgetNotification: the handle of the plugin is its Gossiper

static void widget_shown(LV2_Handle instance, const char* symbol, ALABS_HMI_Widget widget, const ALABS_HMI_WidgetArea* area) {
    Widgets* widgets = (Widgets*) ((Gossiper*) instance)->lv2->widgets;

    if (widgets != NULL) {
        widgets_shown(&widgets->widgets, symbol, widget, area);
    }
}

static void widget_hidden(LV2_Handle instance, ALABS_HMI_Widget widget) {
    Widgets* widgets = (Widgets*) ((Gossiper*) instance)->lv2->widgets;

    if (widgets != NULL) {
        widgets_hidden(&widgets->widgets, widget);
    }
}

const ALABS_HMI_WidgetNotification* LV2_Gossiper_Widgets_notification(void) {
    static const ALABS_HMI_WidgetNotification notification = {
        sizeof(ALABS_HMI_WidgetNotification),
        widget_shown,
        widget_hidden,
    };

    return &notification;
}
