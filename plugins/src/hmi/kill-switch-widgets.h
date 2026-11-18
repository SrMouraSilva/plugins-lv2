/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The display widgets of Kill Switch (`pattern`): the pattern, the step that plays and the tempo, for devices with the
 * aLabs HMI widgets extension (see hmi/widget-host.h). The plugin fills `KillSwitchView` on every cycle and calls
 * `kill_switch_widgets_run`; everything else (what is on a display, what is drawn, when) is here.
 */

#ifndef HMI_KILL_SWITCH_WIDGETS_H
#define HMI_KILL_SWITCH_WIDGETS_H

#include <stdbool.h>
#include <stdint.h>

#include "widget-host.h"

/** The most steps a pattern has: the same number as the custom pattern of the plugin (checked there) */
#define KILL_SWITCH_WIDGET_MAX_STEPS 16

/** A pattern with more steps a second than this cannot be followed with the eye: the widget does not show the step */
#define KILL_SWITCH_WIDGET_MAX_FOLLOW_STEPS_PER_SECOND 10.0

/** What the widgets draw, as run() saw it on its last cycle */
typedef struct {
    bool  bypassed;
    char  name[24];
    char  steps[KILL_SWITCH_WIDGET_MAX_STEPS + 1];
    int   count;
    int   step;
    float bpm;
    /** 0..3: the Speed multiplier */
    int   speed;
    /** Percent */
    int   depth;
    /** Too many steps a second to follow */
    bool  fast;
} KillSwitchView;

typedef struct {
    WidgetHost     host;
    KillSwitchView view;
} KillSwitchWidgets;

/** In instantiate(): `features` is what the host gave */
extern void kill_switch_widgets_init(KillSwitchWidgets* widgets, const LV2_Feature* const* features);

/** At the end of run(), once the view is up to date: draw what changed on the widgets that are on a display */
extern void kill_switch_widgets_run(KillSwitchWidgets* widgets, uint32_t frames, double sample_rate);

#endif
