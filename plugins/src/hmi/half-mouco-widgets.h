/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The display widgets of Half Mouco (`stereo`): a row of steps for each side, or the curve of the ending along its time, for
 * devices with the aLabs HMI widgets extension (see hmi/widget-host.h). The plugin fills `HalfMoucoView` on every cycle and
 * calls `half_mouco_widgets_run`; everything else (what is on a display, what is drawn, when) is here. The only thing the widget
 * asks of the plugin is `ending_gate`, to draw the curve of an ending from the same function that plays it.
 */

#ifndef HMI_HALF_MOUCO_WIDGETS_H
#define HMI_HALF_MOUCO_WIDGETS_H

#include <stdbool.h>
#include <stdint.h>

#include "widget-host.h"

/** The most steps a pattern has: the same number as the custom pattern of the plugin (checked there) */
#define HALF_MOUCO_WIDGET_MAX_STEPS 16

/** A pattern with more steps a second than this cannot be followed with the eye: the widget does not show the step */
#define HALF_MOUCO_WIDGET_MAX_FOLLOW_STEPS_PER_SECOND 10.0

/** What an ending remembers between two calls (the index of the bounce or of the blade): only a cache */
typedef struct {
    int index;
} HalfMoucoEndingState;

/**
 * The gate (0 = cut .. 1 = pass) of each side at the time `t` of an ending of `total` seconds, with `beat` seconds a beat:
 * the function of the plugin that plays the endings. `ending` is the index of the `ending` port.
 */
typedef void (*HalfMoucoEndingGate)(int ending, double t, double total, double beat, HalfMoucoEndingState* state, float* gate_l, float* gate_r);

/** What the widgets draw, as run() saw it on its last cycle */
typedef struct {
    bool   bypassed;
    /** The Endings mode: the widget draws the curve of the ending (`ending`, `elapsed` of `total` seconds); otherwise the steps */
    bool   endings;
    /** The name of the pattern (an ending's name is the widget's) */
    char   name[24];
    char   left[HALF_MOUCO_WIDGET_MAX_STEPS + 1];
    char   right[HALF_MOUCO_WIDGET_MAX_STEPS + 1];
    int    count;
    int    step;
    float  bpm;
    /** 0..3: the Speed multiplier */
    int    speed;
    /** Too many steps a second to follow */
    bool   fast;
    int    ending;
    double elapsed;
    double total;
    /** The length of the ending, in beats */
    float  length;
    float  floor_gain;
} HalfMoucoView;

typedef struct {
    WidgetHost          host;
    HalfMoucoView       view;
    HalfMoucoEndingGate gate;
} HalfMoucoWidgets;

/** In instantiate(): `features` is what the host gave */
extern void half_mouco_widgets_init(HalfMoucoWidgets* widgets, const LV2_Feature* const* features, HalfMoucoEndingGate gate);

/** At the end of run(), once the view is up to date: draw what changed on the widgets that are on a display */
extern void half_mouco_widgets_run(HalfMoucoWidgets* widgets, uint32_t frames, double sample_rate);

#endif
