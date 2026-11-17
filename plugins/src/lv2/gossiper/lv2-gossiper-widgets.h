/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LV2_GOSSIPER_WIDGETS_H
#define	LV2_GOSSIPER_WIDGETS_H

#include "../../model/gossiper.h"
#include "../../hmi/alabs-hmi-widgets.h"

/** The widgets of the display (aLabs HMI widgets extension, optional): the order of the `ahw:widget`s of the TTL */
typedef enum {
    WIDGET_FOOTSWITCHES = 0,
    WIDGET_POTENTIOMETERS,
    WIDGET_COUNT
} WidgetIndex;

/** Look for the feature and start the widgets, in instantiate() */
extern void LV2_Gossiper_Widgets_initialize(Gossiper* self, const LV2_Feature* const* features, double sample_rate);

/** After the run of the model: draw what changed on the widgets that are on a display */
extern void LV2_Gossiper_Widgets_run(Gossiper* self, uint32_t n_samples);

/**
 * The host (from a non real-time thread) addressed footswitch `footswitch` (0-based) to a control of the device, or took it
 * away: a small area of the `footswitches` widget shows only the ones that are
 */
extern void LV2_Gossiper_Widgets_set_addressed(Gossiper* self, unsigned int footswitch, bool addressed);

/** Free what LV2_Gossiper_Widgets_initialize allocated */
extern void LV2_Gossiper_Widgets_cleanup(Gossiper* self);

extern const ALABS_HMI_WidgetNotification* LV2_Gossiper_Widgets_notification(void);

#endif
