/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CONFIG_H
#define	CONFIG_H

/*
 * One code base builds both switch plugins. Which one is chosen at compile time
 * (see plugins/Makefile): Tetr4 Switch by default, Hept7 Switch with -DHEPT7_SWITCH.
 */
#ifdef HEPT7_SWITCH

#define PLUGIN_NAME "Hept7 Switch"
#define SWITCH_PLUGIN_URI "http://srmourasilva.github.io/plugins/hept7-switch"

/** Total of presets. The number of footswitches is also related: 7, as many as the colours of the HMI LEDs */
#define TOTAL_PRESETS 7
/** How many CV outputs there are (and so dip switches per preset) */
#define TOTAL_OUTPUTS 8

#else

#define PLUGIN_NAME "Tetr4 Switch"
#define SWITCH_PLUGIN_URI "http://srmourasilva.github.io/plugins/tetr4-switch"

/** Total of presets. The number of footswitches is also related */
#define TOTAL_PRESETS 4
/** How many CV outputs there are */
#define TOTAL_OUTPUTS 4

#endif

#define PRESET_LABEL_MAX_SIZE 16

#define TOTAL_CONTROLLER_NOTIFIERS 4

#endif
