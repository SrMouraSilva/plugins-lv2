/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LV2_WIDGETS_H
#define	LV2_WIDGETS_H

#include "../../model/controller.h"
#include "../../hmi/alabs-hmi-widgets.h"

/** The widgets of the display (aLabs HMI widgets extension, optional): the order of the `ahw:widget`s of the TTL */
typedef enum {
    WIDGET_PRESET = 0,
    WIDGET_COUNT
} WidgetIndex;

/** Look for the feature and start the widgets: `LV2_Controller_initialize` does it */
extern void LV2_Widgets_initialize(Controller* self, const LV2_Feature* const* features, double sample_rate);

/** After Controller_run(): draw what changed on the widgets that are on a display */
extern void LV2_Widgets_run(Controller* self, uint32_t n_samples);

extern const ALABS_HMI_WidgetNotification* LV2_Widgets_notification(void);

#endif
