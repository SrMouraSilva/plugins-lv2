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
#define PLUGIN_URI "http://srmourasilva.github.io/plugins/hept7-switch"

/** Total of presets. The number of footswitches is also related (7: one per HMI LED colour) */
#define TOTAL_PRESETS 7
/** How many CV outputs there are */
#define TOTAL_OUTPUTS 7
/**
 * Dip switches of each preset: one per CV output, plus a last one that turns on/off the HMI pop-up
 * shown when the preset is selected
 */
#define TOTAL_DIPS 8

#else

#define PLUGIN_NAME "Tetr4 Switch"
#define PLUGIN_URI "http://srmourasilva.github.io/plugins/tetr4-switch"

/** Total of presets. The number of footswitches is also related */
#define TOTAL_PRESETS 4
/** How many CV outputs there are */
#define TOTAL_OUTPUTS 4
/** Dip switches of each preset: one per CV output */
#define TOTAL_DIPS 4

#endif

/** Does each preset have a dip switch (after the CV outputs ones) to turn on/off its HMI pop-up? */
#define HAS_POPUP_SWITCH (TOTAL_DIPS > TOTAL_OUTPUTS)

#define PRESET_LABEL_MAX_SIZE 16

#define TOTAL_CONTROLLER_NOTIFIERS 4

#endif
