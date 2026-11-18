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

/** The three buttons of Hept7 Switch, as bits of Controller.button_mask */
#define BUTTON_CURRENT  (1u << 0)
#define BUTTON_NEXT     (1u << 1)
#define BUTTON_PREVIOUS (1u << 2)

/** Tension when is on */
#define MAX_TENSION 10 // volts


typedef struct {
    // Ports
    float* output_cvs[TOTAL_OUTPUTS];
    float* preset_selectors[TOTAL_PRESETS];
    /**
     * Tetr4 Switch: the enumeration "Current preset". Hept7 Switch: the button of that name, which loops forward
     * through the cycle (the symbol is the same, its meaning is not)
     */
    float* current_preset_index;
    /** Hept7 Switch only (NULL on Tetr4 Switch): the buttons that go to the next and to the previous preset in the cycle */
    float* next_preset_button;
    float* previous_preset_button;
    float* preset_outputs[TOTAL_PRESETS][TOTAL_OUTPUTS];
    /** NULL when the plugin has no such port (Tetr4 Switch): the preset is in the cycle */
    float* presets_in_cycle[TOTAL_PRESETS];
    /**
     * Output (NULL when the plugin has no such port): the active preset. Unlike the selectors and "Current preset",
     * the host reports it whenever it changes, whoever changed it, which makes it what the interface follows
     */
    float* active_preset_output;

    /**
     * MIDI (NULL when the host does not connect these ports, in which case nothing is sent). The control
     * change number of each CV output, the channel (0-15) and whether to leave out the ones that did not change
     */
    float* midi_cc[TOTAL_OUTPUTS];
    float* midi_channel;
    float* midi_changes_only;

    /** The values (0 or 127) last sent, for "changes only" */
    uint8_t midi_last_sent[TOTAL_OUTPUTS];

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

    /**
     * Something the widgets show, other than the active preset, changed (a preset's name, by a patch:Set or a restored
     * state; the cycle, which changes where "Next preset" and "Previous preset" lead) and the display has not been told
     * yet: the HMI sends the labels again. Set by the model, cleared by the HMI code
     */
    bool labels_changed;

    /** run() has not run yet: the first cycle reads the pedalboard being loaded */
    bool started;

    /**
     * Presets that "Current preset" goes through (bit N: preset N+1), read on every cycle. 0 when the plugin has no
     * "In the cycle" ports (Tetr4 Switch): "Current preset" then simply selects the preset it names
     * (see Controller_has_cycle). When every one of those ports is off, every preset is in the cycle
     */
    unsigned int cycle_mask;

    /**
     * Hept7 Switch: "Current preset", "Next preset" and "Previous preset" are buttons (BUTTON_* bits). The ones that
     * were on when the previous cycle was read: a press is a button going on, not being on
     */
    unsigned int button_mask;

    /** run() has run at least once and the cycle was read then: a change of the cycle after that is news */
    bool started_once;

    /** The combobox was moved from outside on this cycle (Tetr4 Switch): the device has drawn its own text, which the HMI draws over */
    bool combobox_moved;

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
 * Hept7 Switch (it has the "In the cycle" ports): "Current preset", "Next preset" and "Previous preset" are buttons that
 * step through the presets that are in the cycle, from the active preset
 */
extern bool Controller_has_cycle(const Controller* self);

/** The preset a step of @p direction (+1: next, -1: previous) from the active preset leads to, in the cycle */
extern unsigned int Controller_cycle_target(const Controller* self, int direction);

#endif
