/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The display widgets of Kill Switch: see kill-switch-widgets.h.
 */

#include <stdio.h>
#include <string.h>

#include "kill-switch-widgets.h"
#include "widget-steps.h"

/** The widgets of the display: the order of WIDGET_SYMBOLS and of the `ahw:widget`s of kill-switch.ttl */
typedef enum {
    WIDGET_PATTERN = 0,
    WIDGET_COUNT
} WidgetIndex;

static const char* const WIDGET_SYMBOLS[WIDGET_COUNT] = { "pattern" };

void kill_switch_widgets_init(KillSwitchWidgets* widgets, const LV2_Feature* const* features) {
    memset(&widgets->view, 0, sizeof(widgets->view));
    widgets_init(&widgets->host, widgets_find_feature(features), WIDGET_SYMBOLS, WIDGET_COUNT);
}

/** The pattern: its name, the steps with the one that plays, the tempo (and Depth, where there is room) */
static void render_pattern(const KillSwitchView* view, Canvas* canvas) {
    const int w = canvas->width;
    const int h = canvas->height;
    char text[24];

    if (h < 14) {
        // A footer: only the row of steps (and the tempo when it is wide enough)
        int left = 0;
        if (w >= 90) {
            snprintf(text, sizeof(text), "%d BPM", (int) (view->bpm + 0.5f));
            canvas_text(canvas, 0, (h - 5) / 2, FONT_STYLE_SMALL, text, true);
            left = 34;
        }
        widget_draw_steps(canvas, left, 0, w - left, h - STEPS_CARET_HEIGHT, view->steps, view->count, view->step, view->fast, view->bypassed);
        return;
    }

    if (h < 30) {
        int top = 0;
        if (h >= 22) {
            canvas_text_fit(canvas, 0, 0, w - (view->bypassed ? 14 : 0), false, view->name, true);
            top = 7;
        }
        if (view->bypassed && h >= 22) {
            canvas_text_right(canvas, w, 0, FONT_STYLE_SMALL, "OFF", true);
        }
        widget_draw_steps(canvas, 0, top, w, h - top - STEPS_CARET_HEIGHT, view->steps, view->count, view->step, view->fast, view->bypassed);
        return;
    }

    // A column, two columns or the whole screen: name, steps, tempo
    const int bottom = h - 5;
    int right = w;

    if (view->bypassed) {
        canvas_text_right(canvas, w, 1, FONT_STYLE_SMALL, "OFF", true);
        right = w - 16;
    }
    canvas_text_fit(canvas, 0, 0, right, true, view->name, true);

    int extra = 0;
    if (h >= 50) {
        extra = 8;
        snprintf(text, sizeof(text), "DEPTH %d%%", view->depth);
        canvas_text(canvas, 0, bottom - 8, FONT_STYLE_SMALL, text, true);
    }

    const int top = 10;
    const int caret_top = bottom - 4 - extra;
    widget_draw_steps(canvas, 0, top, w, caret_top - top - 1, view->steps, view->count, view->step, view->fast, view->bypassed);

    snprintf(text, sizeof(text), "%d BPM", (int) (view->bpm + 0.5f));
    canvas_text(canvas, 0, bottom, FONT_STYLE_SMALL, text, true);

    static const char* const SPEED_TEXT[] = { "X0.5", "X1", "X2", "X4" };
    canvas_text_right(canvas, w, bottom, FONT_STYLE_SMALL, SPEED_TEXT[view->speed], true);
}

static void render_widget(const void* plugin, int widget, Canvas* canvas) {
    if (widget == WIDGET_PATTERN) {
        render_pattern(&((const KillSwitchWidgets*) plugin)->view, canvas);
    }
}

void kill_switch_widgets_run(KillSwitchWidgets* widgets, uint32_t frames, double sample_rate) {
    if (widgets->host.draw == NULL) {
        return;
    }

    const KillSwitchView* view = &widgets->view;
    uint32_t signature = WIDGET_HASH_SEED;

    signature = widget_hash(signature, (uint32_t) view->bypassed);
    signature = widget_hash(signature, (uint32_t) view->count);
    signature = widget_hash(signature, (uint32_t) (view->fast ? -1 : view->step));
    signature = widget_hash(signature, (uint32_t) (view->bpm + 0.5f));
    signature = widget_hash(signature, (uint32_t) view->speed);
    signature = widget_hash(signature, (uint32_t) view->depth);
    for (int i = 0; i < view->count; ++i) {
        signature = widget_hash(signature, (uint32_t) view->steps[i]);
    }
    for (const char* c = view->name; *c != '\0'; ++c) {
        signature = widget_hash(signature, (uint32_t) *c);
    }

    const uint32_t signatures[WIDGET_COUNT] = { signature };
    widgets_run(&widgets->host, frames, sample_rate, signatures, render_widget, widgets);
}

