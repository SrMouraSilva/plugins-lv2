/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The display widgets of Tetr4 Switch and Hept7 Switch (the sizes come from config.h): `preset`, the preset that is on, its
 * name and what it switches. See hmi/widget-host.h for how a widget is drawn.
 *
 * The device shows the preset only where the musician put a widget: it needs no control addressed (the pop-ups need an
 * `Assign to notify`), so it is also the way to see the preset on a device whose HMI does not let the plugin write the LEDs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../hmi/widget-host.h"
#include "../../model/controller.h"
#include "lv2-widgets.h"

static const char* const WIDGET_SYMBOLS[WIDGET_COUNT] = { "preset" };

/** What the widget draws: the picture of the state on the last cycle */
typedef struct {
    unsigned int current;
    char         labels[TOTAL_PRESETS][PRESET_LABEL_MAX_SIZE];
    /** Bit n: the dip switch of output n is on, for each preset */
    unsigned int outputs[TOTAL_PRESETS];
} View;

typedef struct {
    WidgetHost widgets;
    double     sample_rate;
    View       view;
} Widgets;

/** One for each Controller (a pointer from the LV2 part, so the model stays free of the widgets) */
static Widgets* widgets_of(Controller* self) {
    return (Widgets*) self->lv2->widgets;
}

void LV2_Widgets_initialize(Controller* self, const LV2_Feature* const* features, double sample_rate) {
    Widgets* widgets = (Widgets*) calloc(1, sizeof(Widgets));

    // Without room for them the widgets are simply not drawn
    self->lv2->widgets = widgets;

    if (widgets == NULL) {
        return;
    }

    widgets->sample_rate = sample_rate;
    widgets_init(&widgets->widgets, widgets_find_feature(features), WIDGET_SYMBOLS, WIDGET_COUNT);
}

// ---------------------------------------------------------------------------------------------------------------------
// Drawing

/** A square for an output: lit when the dip switch is on, an outline when it is off */
static void draw_output(Canvas* canvas, int x, int y, int size, bool on) {
    if (on) {
        canvas_fill(canvas, x, y, size, size, true);
    } else {
        canvas_frame(canvas, x, y, size, size);
    }
}

/** The outputs of a preset in a row of `size` squares, from `x` (they take TOTAL_OUTPUTS * (size + gap) - gap) */
static void draw_outputs_row(Canvas* canvas, int x, int y, int size, int gap, unsigned int outputs) {
    for (unsigned int o = 0; o < TOTAL_OUTPUTS; ++o) {
        draw_output(canvas, x + (int) o * (size + gap), y, size, (outputs >> o) & 1u);
    }
}

static void number_text(char* text, size_t size, unsigned int preset) {
    snprintf(text, size, "%u", preset + 1);
}

/** A preset number on an inverted square: the number of a footswitch */
static void draw_number_box(Canvas* canvas, int x, int y, int size, unsigned int preset, bool filled) {
    char text[4];

    number_text(text, sizeof(text), preset);
    if (filled) {
        canvas_fill(canvas, x, y, size, size, true);
    } else {
        canvas_frame(canvas, x, y, size, size);
    }
    canvas_text_centred(canvas, x, x + size, y + (size - 5) / 2, FONT_STYLE_SMALL, text, !filled);
}

/** One preset in a row of `height` pixels: its number, its name and what it switches */
static void draw_list_row(Canvas* canvas, const View* view, unsigned int preset, int y, int height) {
    const int w = canvas->width;
    const bool active = preset == view->current;
    const int outputs_width = TOTAL_OUTPUTS * 4 - 1;
    char text[4];

    if (active) {
        canvas_fill(canvas, 0, y, w, height, true);
    }

    number_text(text, sizeof(text), preset);
    const bool big = height >= 9;
    const FontStyle style = big ? FONT_STYLE_BIG : FONT_STYLE_SMALL;
    const int text_y = y + (height - canvas_font_height(style)) / 2;

    canvas_text(canvas, 2, text_y, style, text, !active);
    canvas_text_fit(canvas, 2 + canvas_char_width(style) + 3, text_y, w - 2 - canvas_char_width(style) - 3 - outputs_width - 5, big,
                    view->labels[preset], !active);

    for (unsigned int o = 0; o < TOTAL_OUTPUTS; ++o) {
        const int x = w - 2 - outputs_width + (int) o * 4;
        const int dot_y = y + (height - 3) / 2;

        if ((view->outputs[preset] >> o) & 1u) {
            canvas_fill(canvas, x, dot_y, 3, 3, !active);
        } else {
            canvas_pixel(canvas, x + 1, dot_y + 1, !active);
        }
    }
}

static void render_preset(const Controller* controller, const View* view, Canvas* canvas) {
    const int w = canvas->width;
    const int h = canvas->height;
    char text[4];
    (void) controller;

    // Tall enough for a line a preset: all of them, the active one inverted
    if (w >= 90 && h >= 8 * TOTAL_PRESETS) {
        const int row = h / TOTAL_PRESETS;
        const int top = (h - row * TOTAL_PRESETS) / 2;

        for (unsigned int p = 0; p < TOTAL_PRESETS; ++p) {
            draw_list_row(canvas, view, p, top + (int) p * row, row);
        }
        return;
    }

    // A footer, or a row of a knob: the number, the name, and what it switches when there is room
    if (h < 22) {
        const int size = h >= 9 ? 9 : 7;
        const int y = (h - size) / 2;
        const int name_y = (h - (h >= 8 ? 7 : 5)) / 2;
        int right = w;

        draw_number_box(canvas, 0, y, size, view->current, true);
        if (w >= 90) {
            right = w - (TOTAL_OUTPUTS * 4 - 1) - 2;
            draw_outputs_row(canvas, right + 2, (h - 3) / 2, 3, 1, view->outputs[view->current]);
        }
        canvas_text_fit(canvas, size + 3, name_y, right - size - 3 - (w >= 90 ? 3 : 0), h >= 8, view->labels[view->current], true);
        return;
    }

    // A column: the number, what it switches, the name, and one box for each preset
    if (w < 60) {
        const int digit_x = 0;

        number_text(text, sizeof(text), view->current);
        canvas_big_digit(canvas, digit_x, 0, (int) view->current + 1, true);

        // The outputs in two rows beside the number: 4 columns of 5 pixels fit in what is left of 40
        const int columns = (TOTAL_OUTPUTS + 1) / 2;
        const int cell = w >= 40 ? 6 : 5;
        const int origin = w - columns * cell + 1;
        for (unsigned int o = 0; o < TOTAL_OUTPUTS; ++o) {
            draw_output(canvas, origin + (int) (o % (unsigned int) columns) * cell, 1 + (int) (o / (unsigned int) columns) * 7, 5, (view->outputs[view->current] >> o) & 1u);
        }

        const int name_y = h >= 30 ? 19 : 17;
        canvas_text_fit(canvas, 0, name_y, w, true, view->labels[view->current], true);

        // The pager: one box for each preset, the active one filled
        const int pager_y = h - 5;
        for (unsigned int p = 0; p < TOTAL_PRESETS; ++p) {
            const int left = (int) (p * (unsigned int) w) / TOTAL_PRESETS;
            const int cell_w = (int) ((p + 1) * (unsigned int) w) / TOTAL_PRESETS - left - 1;

            if (p == view->current) {
                canvas_fill(canvas, left, pager_y, cell_w, 5, true);
            } else {
                canvas_frame(canvas, left, pager_y, cell_w, 5);
            }
        }
        return;
    }

    // Wide but not tall enough for a list: a cell for each preset, the name big, and what it switches
    const int cell_h = 9;
    for (unsigned int p = 0; p < TOTAL_PRESETS; ++p) {
        const int left = (int) (p * (unsigned int) w) / TOTAL_PRESETS;
        const int cell_w = (int) ((p + 1) * (unsigned int) w) / TOTAL_PRESETS - left - 1;

        number_text(text, sizeof(text), p);
        if (p == view->current) {
            canvas_fill(canvas, left, 0, cell_w, cell_h, true);
        } else {
            canvas_frame(canvas, left, 0, cell_w, cell_h);
        }
        canvas_text_centred(canvas, left, left + cell_w, 2, FONT_STYLE_SMALL, text, p != view->current);
    }

    const int name_y = h >= 26 ? 13 : 11;
    canvas_text_fit(canvas, 0, name_y, w, true, view->labels[view->current], true);

    if (h >= name_y + 9 + 7) {
        const int size = 5;
        const int gap = 2;
        const int width = TOTAL_OUTPUTS * (size + gap) - gap;
        draw_outputs_row(canvas, (w - width) / 2, h - size - 1, size, gap, view->outputs[view->current]);
    }
}

static void render_widget(const void* plugin, int widget, Canvas* canvas) {
    const Controller* self = (const Controller*) plugin;

    if (widget == WIDGET_PRESET) {
        render_preset(self, &((const Widgets*) self->lv2->widgets)->view, canvas);
    }
}

void LV2_Widgets_run(Controller* self, uint32_t n_samples) {
    Widgets* widgets = widgets_of(self);

    if (widgets == NULL || widgets->widgets.draw == NULL) {
        return;
    }

    View* view = &widgets->view;
    uint32_t signature = WIDGET_HASH_SEED;

    view->current = self->current_preset;
    signature = widget_hash(signature, view->current);

    for (unsigned int p = 0; p < TOTAL_PRESETS; ++p) {
        snprintf(view->labels[p], sizeof(view->labels[p]), "%s", Controller_get_preset_label(self, p));
        view->outputs[p] = 0;

        for (unsigned int o = 0; o < TOTAL_OUTPUTS; ++o) {
            const float* dip = self->preset_outputs[p][o];

            if (dip != NULL && *dip >= 0.5f) {
                view->outputs[p] |= 1u << o;
            }
        }

        signature = widget_hash(signature, view->outputs[p]);
        for (const char* c = view->labels[p]; *c != '\0'; ++c) {
            signature = widget_hash(signature, (uint32_t) *c);
        }
    }

    const uint32_t signatures[WIDGET_COUNT] = { signature };
    widgets_run(&widgets->widgets, n_samples, widgets->sample_rate, signatures, render_widget, self);
}

// ---------------------------------------------------------------------------------------------------------------------
// WidgetNotification: the handle of the plugin is its Controller

static void widget_shown(LV2_Handle instance, const char* symbol, ALABS_HMI_Widget widget, const ALABS_HMI_WidgetArea* area) {
    Widgets* widgets = widgets_of((Controller*) instance);

    if (widgets != NULL) {
        widgets_shown(&widgets->widgets, symbol, widget, area);
    }
}

static void widget_hidden(LV2_Handle instance, ALABS_HMI_Widget widget) {
    Widgets* widgets = widgets_of((Controller*) instance);

    if (widgets != NULL) {
        widgets_hidden(&widgets->widgets, widget);
    }
}

const ALABS_HMI_WidgetNotification* LV2_Widgets_notification(void) {
    static const ALABS_HMI_WidgetNotification notification = {
        sizeof(ALABS_HMI_WidgetNotification),
        widget_shown,
        widget_hidden,
    };

    return &notification;
}
