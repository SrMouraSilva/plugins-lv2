/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <stdbool.h>

#include "controller.h"
#include "../utils/utils.h"

#define PRESET_LABEL_MAX_LENGTH (PRESET_LABEL_MAX_SIZE - 1)

static void Controller_set_default_preset_label(Controller* self, unsigned int index);
static unsigned int Controller_read_combobox(const Controller* self);
static unsigned int Controller_read_cycle_mask(const Controller* self);
static unsigned int Controller_follow_combobox(const Controller* self, unsigned int combobox);
static unsigned int Controller_next_in_cycle(const Controller* self, unsigned int from, int direction);
static void Controller_update_output_cvs(const Controller* self, uint32_t n_samples);


Controller* Controller_instantiate(void) {
    Controller* self = (Controller*) calloc(1, sizeof(Controller));

    if (self == NULL) {
        return NULL;
    }

    // Same as the TTL defaults: preset 1, with its selector on
    self->current_preset = 0;
    self->previous_preset = 0;
    self->selector_mask = 1u << 0;
    self->combobox = 0;
    self->started = false;
    self->cycle_mask = (1u << TOTAL_PRESETS) - 1;
    self->cycle_position = 0;

    for (unsigned int i=0; i<TOTAL_PRESETS; i++) {
        Controller_set_default_preset_label(self, i);
    }

    self->lv2 = NULL;

    return self;
}

void Controller_cleanup(Controller* self) {
    // Nothing allocated besides the Controller itself, which the caller frees
    (void) self;
}

const char* Controller_get_preset_label(const Controller* self, unsigned int index) {
    if (self == NULL || index >= TOTAL_PRESETS) {
        return NULL;
    }

    return self->preset_labels[index];
}

bool Controller_set_preset_label(Controller* self, unsigned int index, const char* new_label) {
    if (self == NULL || index >= TOTAL_PRESETS) {
        return false;
    }

    if (new_label == NULL || new_label[0] == '\0') {
        Controller_set_default_preset_label(self, index);
    } else {
        sanitize_label_to(self->preset_labels[index], new_label, PRESET_LABEL_MAX_LENGTH);
    }

    return true;
}


void Controller_run(Controller* self, uint32_t n_samples) {
    const unsigned int selectors = make_mask(self->preset_selectors, TOTAL_PRESETS);
    const unsigned int combobox = Controller_read_combobox(self);
    const bool selectors_moved = selectors != self->selector_mask;
    // The plugin's own requests are already in self->combobox: a move is a move from outside
    const bool combobox_moved = combobox != self->combobox;

    self->cycle_mask = Controller_read_cycle_mask(self);

    // Selectors that went on since the previous cycle. Going off never chooses a preset: a momentary footswitch
    // being released, or the active preset being turned off, only asks for its selector to be turned on again
    const unsigned int turned_on = selectors & ~self->selector_mask;
    unsigned int next = self->current_preset;

    if (!self->started) {
        // A pedalboard being loaded (the host does not process while it loads, so its values are all here): the
        // selectors say which preset was on. The combobox may hold where a filtered cycle had left the device
        if (selectors != 0) {
            next = (selectors & (1u << combobox)) ? combobox : lowest_on_bit(selectors);
        } else {
            next = combobox;
        }

    } else if (turned_on != 0) {
        // Several at once (a snapshot): the combobox, which moved with them, breaks the tie
        next = (turned_on & (1u << combobox)) ? combobox : lowest_on_bit(turned_on);

    } else if (combobox_moved && !selectors_moved) {
        // When the selectors moved too (a snapshot, whose combobox may hold a cycle position), they have the last word
        next = Controller_follow_combobox(self, combobox);
    }

    if (combobox_moved || !self->started) {
        self->cycle_position = combobox;
    }

    self->started = true;
    self->inputs_changed = selectors_moved || combobox_moved;
    self->selector_mask = selectors;
    self->combobox = combobox;

    self->preset_changed = next != self->current_preset;

    if (self->preset_changed) {
        self->previous_preset = self->current_preset;
        self->current_preset = next;
    }

    if (self->active_preset_output != NULL) {
        *self->active_preset_output = (float) self->current_preset;
    }

    Controller_update_output_cvs(self, n_samples);
}

bool Controller_has_cycle(const Controller* self) {
    return self->cycle_mask != 0;
}

int Controller_cycle_step(unsigned int from, unsigned int to) {
    if (to == (from + 1) % TOTAL_PRESETS) {
        return 1;
    }
    if (to == (from + TOTAL_PRESETS - 1) % TOTAL_PRESETS) {
        return -1;
    }
    return 0;
}

/** The option opposite to the device's position: the only other value the plugin sets "Current preset" to */
static unsigned int Controller_opposite_position(const Controller* self) {
    return (self->cycle_position + TOTAL_PRESETS / 2) % TOTAL_PRESETS;
}

bool Controller_may_show_in_combobox(const Controller* self, unsigned int preset) {
    return preset == self->cycle_position || preset == Controller_opposite_position(self);
}

/**
 * Where a move of "Current preset" from outside leads. Without a cycle (Tetr4 Switch), to the preset it names. With
 * one (even when every preset is in it: the steps then count from the active preset too):
 *  - a step (the next or previous option: a footswitch, the B+C pair, a knob) goes to the next or previous preset in
 *    the cycle, counted from the active preset, not from the value: the device's position may be behind it
 *  - a jump goes to the preset it names, or, when that preset is out of the cycle, to the next one in the cycle in the
 *    direction of the jump
 *
 * The device moves one option from its own position: the last value it sent (cycle_position), or a value the port
 * had since then, when MOD UI gave it to the device again (it does when it shows the page of the device again). The
 * plugin only ever sets the port to the device's position or to the option opposite to it
 * (Controller_may_show_in_combobox), so the device is at one of those two, which are not next to each other:
 *  - the value of a press is next to only one of them, so its direction is never ambiguous
 *  - it is never the value the port holds, so the host always sees it (a press is never lost)
 */
static unsigned int Controller_follow_combobox(const Controller* self, unsigned int combobox) {
    if (!Controller_has_cycle(self)) {
        return combobox;
    }

    int direction = Controller_cycle_step(self->cycle_position, combobox);

    if (direction == 0) {
        direction = Controller_cycle_step(Controller_opposite_position(self), combobox);
    }

    if (direction != 0) {
        return Controller_next_in_cycle(self, self->current_preset, direction);
    }

    if (self->cycle_mask & (1u << combobox)) {
        return combobox;
    }

    return Controller_next_in_cycle(self, combobox, combobox < self->cycle_position ? -1 : 1);
}

/** The first preset in the cycle after @p from (itself only when it is the only one) */
static unsigned int Controller_next_in_cycle(const Controller* self, unsigned int from, int direction) {
    unsigned int preset = from;

    for (unsigned int i=0; i<TOTAL_PRESETS; i++) {
        preset = (preset + (direction > 0 ? 1 : TOTAL_PRESETS - 1)) % TOTAL_PRESETS;

        if (self->cycle_mask & (1u << preset)) {
            return preset;
        }
    }

    return from;
}

static unsigned int Controller_read_cycle_mask(const Controller* self) {
    unsigned int mask = 0;
    bool has_ports = false;

    for (unsigned int i=0; i<TOTAL_PRESETS; i++) {
        float* const port = self->presets_in_cycle[i];

        if (port == NULL) {
            continue;
        }

        has_ports = true;

        if (!is_zero(*port)) {
            mask |= 1u << i;
        }
    }

    if (!has_ports) {
        return 0;
    }

    // Nothing in the cycle would leave "Current preset" with nowhere to go: take it as everything
    return mask != 0 ? mask : (1u << TOTAL_PRESETS) - 1;
}

/** The combobox port as a preset index, rounded and kept in range whatever the host writes into it */
static unsigned int Controller_read_combobox(const Controller* self) {
    const float value = *self->current_preset_index;

    if (!(value > 0.5f)) {  // also NaN
        return 0;
    }

    const unsigned int index = (unsigned int) (value + 0.5f);

    return index < TOTAL_PRESETS ? index : TOTAL_PRESETS - 1;
}

static void Controller_update_output_cvs(const Controller* self, uint32_t n_samples) {
    float* const* dips = self->preset_outputs[self->current_preset];

    // One output at a time: a plain fill of a contiguous buffer, which the compiler vectorises
    for (unsigned int output = 0; output < TOTAL_OUTPUTS; output++) {
        const float value = is_zero(*dips[output]) ? 0.0f : MAX_TENSION;
        float* const buffer = self->output_cvs[output];

        for (uint32_t i = 0; i < n_samples; i++) {
            buffer[i] = value;
        }
    }
}

static void Controller_set_default_preset_label(Controller* self, unsigned int index) {
    format_default_label(self->preset_labels[index], PRESET_LABEL_MAX_SIZE, "Preset", index);
}
