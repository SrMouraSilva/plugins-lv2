/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The display widgets of Gossiper: `footswitches` (which of the seven are on; a footswitch that changes shows its name and
 * its state over the widget for a moment, like a pop-up) and `potentiometers` (the four, as a percentage). They show what
 * the controllers do on any area of the display, with no `Assign to notify` needed (the pop-ups of the device need one, and
 * are gone after a moment). See hmi/widget-host.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../hmi/widget-host.h"
#include "lv2-gossiper.h"
#include "lv2-gossiper-widgets.h"

static const char* const WIDGET_SYMBOLS[WIDGET_COUNT] = { "footswitches", "potentiometers" };

#define LABEL_MAX 16

/** How long the pop-up of a footswitch that changed stays over the widget */
#define POPUP_SECONDS 1.5
/** Changes in the first moments are the pedalboard loading (a state restored), not a footswitch pressed */
#define QUIET_SECONDS 1.0

typedef struct {
    bool on[TOTAL_GOSSIPER_FOOTSWITCHES];
    /** The footswitch whose pop-up is on (-1: none) and the state it changed to */
    int  popup;
    bool popup_on;
    char labels[TOTAL_GOSSIPER_FOOTSWITCHES][LABEL_MAX];
    int  percent[TOTAL_GOSSIPER_POTENTIOMETER];
    /** Bit n: footswitch n+1 is in use (addressed to a control, or marked in its port; 0: none is known to be) */
    uint32_t addressed;
} View;

typedef struct {
    WidgetHost widgets;
    double     sample_rate;
    View       view;
    /** Samples left of the pop-up, and of the quiet time after the plugin was created */
    uint32_t   popup_left;
    uint32_t   quiet_left;
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
    widgets->quiet_left = (uint32_t) (sample_rate * QUIET_SECONDS);
    widgets->view.popup = -1;
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

/** A footswitch as a box with its number (the big font where the box has room for it): filled when it is on */
static void draw_switch_box(Canvas* canvas, int x, int y, int width, int height, int number, bool on) {
    char text[4];
    const FontStyle style = width >= 8 && height >= 9 ? FONT_STYLE_BIG : FONT_STYLE_SMALL;
    const int text_height = canvas_font_height(style);

    snprintf(text, sizeof(text), "%d", number);

    if (on) {
        canvas_fill(canvas, x, y, width, height, true);
    } else {
        canvas_frame(canvas, x, y, width, height);
    }

    if (width >= canvas_text_width(style, 1) + 2 && height >= text_height + 2) {
        canvas_text_centred(canvas, x, x + width, y + (height - text_height) / 2, style, text, !on);
    }
}

/**
 * A box for a footswitch that is not in use: a dotted outline and its number as clear as any other. The box is only 7 px
 * high in a list, so the number touches its outline: a dot of the outline next to a lit pixel of the number is left out, or
 * the two would run together.
 */
static void draw_dotted_box(Canvas* canvas, int x, int y, int width, int height, int number) {
    char text[4];
    const FontStyle style = width >= 8 && height >= 9 ? FONT_STYLE_BIG : FONT_STYLE_SMALL;
    const int text_height = canvas_font_height(style);

    snprintf(text, sizeof(text), "%d", number);
    canvas_fill(canvas, x, y, width, height, false);

    if (width >= canvas_text_width(style, 1) + 2 && height >= text_height + 2) {
        canvas_text_centred(canvas, x, x + width, y + (height - text_height) / 2, style, text, true);
    }

    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            if (i != 0 && j != 0 && i != width - 1 && j != height - 1) {
                continue;
            }

            // The two pixels of the inside this one touches (one, except in a corner)
            const int in_x = i == 0 ? 1 : (i == width - 1 ? width - 2 : i);
            const int in_y = j == 0 ? 1 : (j == height - 1 ? height - 2 : j);
            const bool touches_number = canvas_get(canvas, x + in_x, y + j) || canvas_get(canvas, x + i, y + in_y);

            canvas_pixel(canvas, x + i, y + j, ((x + i + y + j) & 1) == 0 && !touches_number);
        }
    }
}

/** Is the footswitch known not to be in use? (nothing is known while none is marked) */
static bool is_unassigned(const View* view, int footswitch) {
    return view->addressed != 0 && ((view->addressed >> footswitch) & 1u) == 0;
}

/**
 * The footswitches an area shows, apart from the list of the whole screen: the ones in use when any is known to be (the
 * others are not worth the room), all of them otherwise.
 * @return how many, their indices in `shown`
 */
static int footswitches_shown(const View* view, int* shown) {
    int count = 0;

    for (int i = 0; i < TOTAL_GOSSIPER_FOOTSWITCHES; ++i) {
        if (!is_unassigned(view, i)) {
            shown[count++] = i;
        }
    }

    return count;
}

/**
 * The whole screen: every footswitch with its name and, on the right, its state. One that is not in use has a dotted box
 * (its number as clear as the others') and says so where the state would be.
 */
static void render_list(const View* view, Canvas* canvas) {
    const int w = canvas->width;
    const int h = canvas->height;
    const int total = TOTAL_GOSSIPER_FOOTSWITCHES;
    const int row = h / total;
    bool any_unassigned = false;

    for (int i = 0; i < total; ++i) {
        any_unassigned = any_unassigned || is_unassigned(view, i);
    }

    // The same room for the names in every row
    const int right_width = canvas_text_width(FONT_STYLE_SMALL, any_unassigned ? 10 : 3);

    for (int i = 0; i < total; ++i) {
        const int y = (h - row * total) / 2 + i * row;
        const bool unassigned = is_unassigned(view, i);

        canvas_text_fit(canvas, row + 3, y + (row - 7) / 2, w - row - 3 - right_width - 3, true, view->labels[i], true);

        if (unassigned) {
            draw_dotted_box(canvas, 0, y + 1, row - 1, row - 2, i + 1);
            canvas_text_right(canvas, w, y + (row - 5) / 2, FONT_STYLE_SMALL, "UNASSIGNED", true);
        } else {
            draw_switch_box(canvas, 0, y + 1, row - 1, row - 2, i + 1, view->on[i]);
            canvas_text_right(canvas, w, y + (row - 5) / 2, FONT_STYLE_SMALL, view->on[i] ? "ON" : "OFF", true);
        }
    }
}

/** The pop-up of the footswitch that has just changed: its name and its state, over whatever the area shows */
static void draw_popup(const View* view, Canvas* canvas) {
    if (view->popup < 0) {
        return;
    }

    const int w = canvas->width;
    const int h = canvas->height;
    const char* label = view->labels[view->popup];
    const char* state = view->popup_on ? "ON" : "OFF";

    // Drawn as it is when the footswitch is off (light on dark); when it is on, the whole picture is inverted at the end
    canvas_fill(canvas, 0, 0, w, h, false);

    if (h < 22) {
        // A footer: one line, the name on the left and the state on the right
        canvas_text_right(canvas, w - 1, (h - 5) / 2, FONT_STYLE_SMALL, state, true);
        canvas_text_fit(canvas, 1, (h - 5) / 2, w - 2 - canvas_text_width(FONT_STYLE_SMALL, (int) strlen(state)) - 3, false, label, true);
    } else {
        // A window over the whole area (a box in the middle of a large area left pieces of what is under it showing)
        canvas_frame(canvas, 0, 0, w, h);

        const int length = (int) strlen(label);
        const FontStyle style = canvas_text_width(FONT_STYLE_BIG, length) <= w - 6 ? FONT_STYLE_BIG : FONT_STYLE_SMALL;
        int count = canvas_chars_that_fit(style, w - 6);
        count = count > length ? length : count;

        const int label_height = canvas_font_height(style);
        const int tag_width = canvas_text_width(FONT_STYLE_BIG, (int) strlen(state)) + 6;
        const int content = label_height + 4 + 9;
        const int top = (h - content) / 2;
        const int tag_x = (w - tag_width) / 2;
        const int tag_y = top + label_height + 4;

        canvas_text_n(canvas, (w - canvas_text_width(style, count)) / 2, top, style, label, count, true);
        canvas_frame(canvas, tag_x, tag_y, tag_width, 9);
        canvas_text_centred(canvas, tag_x, tag_x + tag_width, tag_y + 1, FONT_STYLE_BIG, state, true);
    }

    if (view->popup_on) {
        canvas_invert(canvas, 0, 0, w, h);
    }
}

static void render_footswitches(const View* view, Canvas* canvas) {
    const int w = canvas->width;
    const int h = canvas->height;
    int shown[TOTAL_GOSSIPER_FOOTSWITCHES];

    if (w >= 90 && h >= 9 * TOTAL_GOSSIPER_FOOTSWITCHES) {
        render_list(view, canvas);
        draw_popup(view, canvas);
        return;
    }

    const int count = footswitches_shown(view, shown);

    // A footer, or a row of a knob: only the boxes
    if (h < 22) {
        const int box = h > 11 ? 11 : h;

        for (int k = 0; k < count; ++k) {
            const int left = k * w / count;
            draw_switch_box(canvas, left, (h - box) / 2, (k + 1) * w / count - left - 1, box, shown[k] + 1, view->on[shown[k]]);
        }
        draw_popup(view, canvas);
        return;
    }

    // The name of the plugin, and the boxes in the room under it: one row where it is wide enough, rows of four otherwise
    const int top = 9;
    const int gap = 2;
    const int per_row = w >= 80 ? count : (count < 4 ? count : 4);
    const int rows = (count + per_row - 1) / per_row;
    int box_h = (h - top - (rows - 1) * gap) / rows;
    box_h = box_h > 16 ? 16 : box_h;
    const int y0 = top + (h - top - (rows * box_h + (rows - 1) * gap)) / 2;

    canvas_text_fit(canvas, 0, 0, w, true, "Gossiper", true);

    for (int k = 0; k < count; ++k) {
        const int column = k % per_row;
        const int row = k / per_row;
        const int left = column * w / per_row;
        draw_switch_box(canvas, left, y0 + row * (box_h + gap), (column + 1) * w / per_row - left - 1, box_h, shown[k] + 1, view->on[shown[k]]);
    }

    draw_popup(view, canvas);
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

    // In use: addressed to a control of the device (the host tells us for the device's own controls only) or marked
    // in its `in_use` port (the settings page does it for any kind of control; so can anyone else)
    view->addressed = __atomic_load_n(&widgets->addressed, __ATOMIC_ACQUIRE);

    for (int i = 0; i < TOTAL_GOSSIPER_FOOTSWITCHES; ++i) {
        const float* in_use = self->switches[i].in_use;

        if (in_use != NULL && *in_use > 0.5f) {
            view->addressed |= 1u << i;
        }
    }

    widgets->quiet_left = widgets->quiet_left > n_samples ? widgets->quiet_left - n_samples : 0;

    if (widgets->popup_left > n_samples) {
        widgets->popup_left -= n_samples;
    } else {
        widgets->popup_left = 0;
        view->popup = -1;
    }

    for (int i = 0; i < TOTAL_GOSSIPER_FOOTSWITCHES; ++i) {
        const bool on = self->switches[i].activated;

        // A footswitch that changed: its pop-up (the latest one wins), unless the pedalboard is still loading
        if (self->switches[i].updated && widgets->quiet_left == 0) {
            view->popup = i;
            view->popup_on = on;
            widgets->popup_left = (uint32_t) (widgets->sample_rate * POPUP_SECONDS);
        }
        view->on[i] = on;

        const char* label = Gossiper_get_footswitch_label(self, (unsigned int) i);
        snprintf(view->labels[i], sizeof(view->labels[i]), "%s", label != NULL && label[0] != '\0' ? label : "Footswitch");

        switches_signature = widget_hash(switches_signature, (uint32_t) on);
        for (const char* c = view->labels[i]; *c != '\0'; ++c) {
            switches_signature = widget_hash(switches_signature, (uint32_t) *c);
        }
    }
    switches_signature = widget_hash(switches_signature, (uint32_t) (view->popup + 1));
    switches_signature = widget_hash(switches_signature, (uint32_t) view->popup_on);
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
