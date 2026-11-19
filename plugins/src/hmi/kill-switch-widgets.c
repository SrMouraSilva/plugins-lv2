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
    WIDGET_STEREO = 0,
    WIDGET_COUNT
} WidgetIndex;

static const char* const WIDGET_SYMBOLS[WIDGET_COUNT] = { "stereo" };

#define ENDING_COUNT 5

/** The names of the endings: keep them in step with the scale points of `ending` in kill-switch.ttl (and the order of `Ending` in kill-switch.c) */
static const char* const ENDING_NAMES[ENDING_COUNT] = {
    "Bouncing ball", "Helicopter", "Stutter out", "Side fade", "Ping-pong out"
};

void kill_switch_widgets_init(KillSwitchWidgets* widgets, const LV2_Feature* const* features, KillSwitchEndingGate gate) {
    memset(&widgets->view, 0, sizeof(widgets->view));
    widgets->gate = gate;
    widgets_init(&widgets->host, widgets_find_feature(features), WIDGET_SYMBOLS, WIDGET_COUNT);
}

/**
 * The curve of an ending on one side along its whole time: for each column the highest gate in it (so that a blip shorter
 * than a column is still drawn), a line from the baseline up; and a dotted line where the ending is now.
 */
static void draw_ending_side(const KillSwitchWidgets* widgets, Canvas* canvas, int x, int y, int width, int height, bool left_side) {
    const KillSwitchView* view = &widgets->view;
    const double beat = 60.0 / (double) view->bpm;
    KillSwitchEndingState state = { 0 };
    const int now = (int) ((view->elapsed >= view->total ? 1.0 : view->elapsed / view->total) * (width - 1) + 0.5);

    for (int column = 0; column < width; ++column) {
        float level = 0.0f;

        for (int sub = 0; sub < 4; ++sub) {
            float gate_l, gate_r;
            const double t = ((column + (sub + 0.5) / 4.0) / width) * view->total;

            widgets->gate(view->ending, t, view->total, beat, &state, &gate_l, &gate_r);
            const float gate = left_side ? gate_l : gate_r;
            if (gate > level) level = gate;
        }

        const int bar = 1 + (int) (level * (float) (height - 1) + 0.5f);

        if (view->idle && level > 0.0f) {
            for (int j = 0; j < bar; ++j) {
                canvas_pixel(canvas, x + column, y + height - 1 - j, ((column + j) & 1) == 0);
            }
        } else {
            canvas_fill(canvas, x + column, y + height - bar, 1, bar, true);
        }
    }

    if (!view->idle) {
        for (int j = 0; j < height; j += 2) {
            canvas_pixel(canvas, x + now, y + j, !canvas_get(canvas, x + now, y + j));
        }
    }
}

/** The two sides: the steps of the left and of the right one, one above the other, with the step that plays under them */
static void render_stereo(const KillSwitchWidgets* widgets, Canvas* canvas) {
    const KillSwitchView* view = &widgets->view;
    const int w = canvas->width;
    const int h = canvas->height;
    char text[24];

    // Rows: [name] left, right, caret (patterns) [text]
    const bool has_name = h >= 22;
    const bool has_text = h >= 30;
    const bool tag = w >= 40 && h >= 14;
    const int top = has_name ? 8 : 0;
    const int bottom = has_text ? h - 7 : h;
    const int caret = view->endings ? 0 : STEPS_CARET_HEIGHT;
    const int rows = bottom - top - caret;
    const int side_height = (rows - 1) / 2;
    const int left_x = tag ? 5 : 0;
    const int strip_width = w - left_x;

    if (has_name) {
        int right = w;

        if (view->idle) {
            canvas_text_right(canvas, w, 1, FONT_STYLE_SMALL, "OFF", true);
            right = w - 16;
        }
        canvas_text_fit(canvas, 0, 0, right, true, view->endings ? ENDING_NAMES[view->ending] : view->name, true);
    }

    const int left_y = top;
    const int right_y = top + side_height + 1;

    if (tag) {
        canvas_text(canvas, 0, left_y + (side_height - 5) / 2, FONT_STYLE_SMALL, "L", true);
        canvas_text(canvas, 0, right_y + (side_height - 5) / 2, FONT_STYLE_SMALL, "R", true);
    }

    if (view->endings) {
        draw_ending_side(widgets, canvas, left_x, left_y, strip_width, side_height, true);
        draw_ending_side(widgets, canvas, left_x, right_y, strip_width, side_height, false);
    } else {
        widget_draw_steps(canvas, left_x, left_y, strip_width, side_height, view->left, view->count, -1, false, view->idle);
        widget_draw_steps(canvas, left_x, right_y, strip_width, side_height, view->right, view->count, view->step, view->fast, view->idle);
        // Both rows share the step: the caret is under the right one
    }

    if (has_text) {
        // Left and right; the right one only where it does not run into the left one
        const char* right = "";
        static const char* const SPEED_TEXT[] = { "X0.5", "X1", "X2", "X4" };

        if (view->endings) {
            snprintf(text, sizeof(text), "%d BEATS", (int) (view->length + 0.5f));
            right = view->elapsed >= view->total && !view->idle ? "END" : "ENDING";
        } else {
            snprintf(text, sizeof(text), "%d BPM", (int) (view->bpm + 0.5f));
            right = SPEED_TEXT[view->speed];
        }

        canvas_text(canvas, 0, h - 5, FONT_STYLE_SMALL, text, true);
        if (canvas_text_width(FONT_STYLE_SMALL, (int) strlen(text)) + 4 + canvas_text_width(FONT_STYLE_SMALL, (int) strlen(right)) <= w) {
            canvas_text_right(canvas, w, h - 5, FONT_STYLE_SMALL, right, true);
        }
    }
}

static void render_widget(const void* plugin, int widget, Canvas* canvas) {
    if (widget == WIDGET_STEREO) {
        render_stereo((const KillSwitchWidgets*) plugin, canvas);
    }
}

void kill_switch_widgets_run(KillSwitchWidgets* widgets, uint32_t frames, double sample_rate) {
    if (widgets->host.draw == NULL) {
        return;
    }

    const KillSwitchView* view = &widgets->view;
    uint32_t signature = WIDGET_HASH_SEED;

    signature = widget_hash(signature, (uint32_t) view->idle);
    signature = widget_hash(signature, (uint32_t) view->endings);
    signature = widget_hash(signature, (uint32_t) view->count);
    signature = widget_hash(signature, (uint32_t) (view->fast ? -1 : view->step));
    signature = widget_hash(signature, (uint32_t) (view->bpm + 0.5f));
    signature = widget_hash(signature, (uint32_t) view->speed);
    signature = widget_hash(signature, (uint32_t) view->ending);
    signature = widget_hash(signature, (uint32_t) (view->length + 0.5f));
    signature = widget_hash(signature, (uint32_t) (view->floor_gain * 100.0f));
    if (view->endings) {
        // The cursor of the ending moves one column of the widest area at a time
        const double progress = view->elapsed >= view->total ? 1.0 : view->elapsed / view->total;
        signature = widget_hash(signature, (uint32_t) (progress * (CANVAS_MAX_WIDTH - 1) + 0.5));
    }
    for (int i = 0; i < view->count; ++i) {
        signature = widget_hash(signature, (uint32_t) view->left[i]);
        signature = widget_hash(signature, (uint32_t) view->right[i]);
    }
    for (const char* c = view->endings ? "" : view->name; *c != '\0'; ++c) {
        signature = widget_hash(signature, (uint32_t) *c);
    }

    const uint32_t signatures[WIDGET_COUNT] = { signature };
    widgets_run(&widgets->host, frames, sample_rate, signatures, render_widget, widgets);
}

