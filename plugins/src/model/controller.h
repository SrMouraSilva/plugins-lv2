/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CONTROLLER_H
#define	CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>
#include "../config.h"

#include "../lv2/controller/lv2-controller.h"

/** Tension when is on */
#define MAX_TENSION 10 // volts


typedef struct {
    // Ports
    float* output_cvs[TOTAL_OUTPUTS];
    float* preset_selectors[TOTAL_PRESETS];
    float* current_preset_index;
    float* preset_outputs[TOTAL_PRESETS][TOTAL_OUTPUTS];
    /** NULL when the plugin has no such port (Tetr4 Switch): the preset is in the cycle */
    float* presets_in_cycle[TOTAL_PRESETS];
    /**
     * Output (NULL when the plugin has no such port): the active preset. Unlike the selectors and "Current preset",
     * the host reports it whenever it changes, whoever changed it, which makes it what the interface follows
     */
    float* active_preset_output;

    /**
     * Fixed size, so changing a label (a patch:Set, applied in run()) allocates nothing
     */
    char preset_labels[TOTAL_PRESETS][PRESET_LABEL_MAX_SIZE];

    /** Active preset (0-based) */
    unsigned int current_preset;

    /** Preset that was active before the last change (0-based) */
    unsigned int previous_preset;

    /** A different preset became active on this cycle */
    bool preset_changed;

    /**
     * The selectors or the combobox moved on this cycle (from the host, or because the plugin asked for it), so they
     * must be checked against the active preset. A momentary footswitch being released is one of these: the selector
     * of the active preset goes off and has to be turned on again, but the preset did not change
     */
    bool inputs_changed;

    /**
     * The selectors and the combobox as they were read on this cycle. The next cycle compares with them: a preset is
     * chosen by a selector turning on or by the combobox moving, not by their level
     */
    unsigned int selector_mask;
    unsigned int combobox;

    /** run() has not run yet: the first cycle reads the pedalboard being loaded */
    bool started;

    /**
     * Presets that "Current preset" goes through (bit N: preset N+1), read on every cycle. 0 when the plugin has no
     * "In the cycle" ports (Tetr4 Switch): "Current preset" then simply selects the preset it names
     * (see Controller_has_cycle). When every one of those ports is off, every preset is in the cycle
     */
    unsigned int cycle_mask;

    /**
     * The last value written into "Current preset" from outside: where the device thinks it is. A footswitch (or the
     * B+C pair, or a knob) addressed to it computes the next value from its own position, which nobody updates when the
     * plugin changes the preset, so a move of one step from here is a step, wherever the preset is
     */
    unsigned int cycle_position;

    LV2_Controller* lv2;
} Controller;


extern Controller* Controller_instantiate(void);
extern void Controller_cleanup(Controller* self);
extern const char* Controller_get_preset_label(const Controller* self, unsigned int index);
extern bool Controller_set_preset_label(Controller* self, unsigned int index, const char* new_label);

/**
 * Read the selectors and the combobox, choose the active preset and write the CV outputs.
 * Run just once per cycle.
 */
extern void Controller_run(Controller* self, uint32_t n_samples);

/**
 * "Current preset" goes through a cycle (Hept7 Switch, which has the "In the cycle" ports): a move of one option is a
 * step to the next or previous preset in the cycle, counted from the active preset (see Controller_follow_combobox)
 */
extern bool Controller_has_cycle(const Controller* self);

/** +1 when @p to is the option after @p from (wrapping around), -1 when it is the one before, 0 otherwise */
extern int Controller_cycle_step(unsigned int from, unsigned int to);

/**
 * With a cycle, may "Current preset" be set to @p preset? Only when it is the device's position or the
 * option opposite to it (see Controller_follow_combobox)
 */
extern bool Controller_may_show_in_combobox(const Controller* self, unsigned int preset);

#endif
