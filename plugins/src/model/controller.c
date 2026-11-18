/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <lv2/lv2plug.in/ns/ext/atom/util.h>

#include "controller.h"
#include "../utils/utils.h"

#define PRESET_LABEL_MAX_LENGTH (PRESET_LABEL_MAX_SIZE - 1)

static void Controller_set_default_preset_label(Controller* self, unsigned int index);
static unsigned int Controller_read_combobox(const Controller* self);
static unsigned int Controller_read_cycle_mask(const Controller* self);
static unsigned int Controller_next_in_cycle(const Controller* self, unsigned int from, int direction);
static unsigned int Controller_read_buttons(const Controller* self);
static void Controller_update_output_cvs(const Controller* self, uint32_t n_samples);
static void Controller_send_midi(Controller* self);


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
    // Read from the ports on the first cycle (0: the plugin has none, so far as anyone has asked)
    self->cycle_mask = 0;

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

    self->labels_changed = true;

    return true;
}


void Controller_run(Controller* self, uint32_t n_samples) {
    const unsigned int selectors = make_mask(self->preset_selectors, TOTAL_PRESETS);
    const bool selectors_moved = selectors != self->selector_mask;
    const unsigned int old_cycle_mask = self->cycle_mask;

    self->cycle_mask = Controller_read_cycle_mask(self);

    // Hept7 Switch: "Current preset", "Next preset" and "Previous preset" are buttons. Tetr4 Switch: it is an enumeration
    const bool buttons = Controller_has_cycle(self);

    // Only the plugin's own requests are already in self->combobox: a move is a move from outside
    const unsigned int combobox = buttons ? self->combobox : Controller_read_combobox(self);
    const bool combobox_moved = combobox != self->combobox;

    // Buttons that went on since the previous cycle. They are momentary-off: held down they are off, and they go on when
    // released, which is when they act. What a pedalboard being loaded holds is not a press
    const unsigned int buttons_now = buttons ? Controller_read_buttons(self) : 0;
    const unsigned int pressed = self->started ? buttons_now & ~self->button_mask : 0;

    // Selectors that went on since the previous cycle. Going off never chooses a preset: a momentary footswitch
    // being released, or the active preset being turned off, only asks for its selector to be turned on again
    const unsigned int turned_on = selectors & ~self->selector_mask;
    unsigned int next = self->current_preset;

    if (!self->started) {
        // A pedalboard being loaded (the host does not process while it loads, so its values are all here): the
        // selectors say which preset was on
        if (selectors != 0) {
            next = (!buttons && (selectors & (1u << combobox))) ? combobox : lowest_on_bit(selectors);
        } else {
            next = buttons ? 0 : combobox;
        }

    } else if (turned_on != 0) {
        // Several at once (a snapshot): the combobox, which moved with them, breaks the tie
        next = (!buttons && (turned_on & (1u << combobox))) ? combobox : lowest_on_bit(turned_on);

    } else if (buttons) {
        // A step in the cycle from the active preset (the one on, in the direction asked). "Current preset" loops
        // forward, like "Next preset". Both directions at once cancel out
        const int direction = ((pressed & (BUTTON_CURRENT | BUTTON_NEXT)) ? 1 : 0) - ((pressed & BUTTON_PREVIOUS) ? 1 : 0);

        if (direction != 0) {
            next = Controller_next_in_cycle(self, self->current_preset, direction);
        }

    } else if (combobox_moved && !selectors_moved) {
        // When the selectors moved too (a snapshot), they have the last word
        next = combobox;
    }

    self->started = true;

    // The device draws the LED and the text of a button again each time it is pressed and released: whatever the widgets
    // showed has to be sent again, after the release (which is when the button acts), even when the preset did not change
    if (pressed != 0) {
        self->labels_changed = true;
    }

    self->inputs_changed = selectors_moved || combobox_moved || pressed != 0;
    self->combobox_moved = combobox_moved;
    self->selector_mask = selectors;
    self->combobox = combobox;
    self->button_mask = buttons_now;

    // What the buttons announce (where they lead) depends on the cycle too
    if (old_cycle_mask != self->cycle_mask && self->started_once) {
        self->labels_changed = true;
    }
    self->started_once = true;

    self->preset_changed = next != self->current_preset;

    if (self->preset_changed) {
        self->previous_preset = self->current_preset;
        self->current_preset = next;
    }

    if (self->active_preset_output != NULL) {
        *self->active_preset_output = (float) self->current_preset;
    }

    Controller_update_output_cvs(self, n_samples);
    Controller_send_midi(self);
}

bool Controller_has_cycle(const Controller* self) {
    return self->cycle_mask != 0;
}

unsigned int Controller_cycle_target(const Controller* self, int direction) {
    return Controller_next_in_cycle(self, self->current_preset, direction);
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

/** Which of the three buttons are on, as BUTTON_* bits (a port the host does not connect is off) */
static unsigned int Controller_read_buttons(const Controller* self) {
    unsigned int mask = 0;

    if (self->current_preset_index != NULL && !is_zero(*self->current_preset_index)) {
        mask |= BUTTON_CURRENT;
    }
    if (self->next_preset_button != NULL && !is_zero(*self->next_preset_button)) {
        mask |= BUTTON_NEXT;
    }
    if (self->previous_preset_button != NULL && !is_zero(*self->previous_preset_button)) {
        mask |= BUTTON_PREVIOUS;
    }

    return mask;
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

/** A control change number, channel or such, as an integer of 0 to @p max whatever the host writes into the port */
static uint8_t Controller_read_midi_value(const float* port, uint8_t max) {
    const float value = *port;

    if (!(value > 0.5f)) {  // also NaN
        return 0;
    }

    return value >= (float) max ? max : (uint8_t) (value + 0.5f);
}

/**
 * The control changes of the preset that became active: one per CV output, 127 when it is on in that preset and 0
 * when it is off, or only the ones that differ from the last one sent (changes only). Nothing when the preset did not
 * change. The buffer is written on every cycle, as the host reads whatever it holds
 */
static void Controller_send_midi(Controller* self) {
    LV2_Atom_Sequence* const out = self->lv2->midi_out;

    if (out == NULL) {
        return;
    }

    // On entry the size of an output sequence is the room the host gives it
    const uint32_t capacity = out->atom.size;

    out->atom.type = self->lv2->uris.atom_Sequence;
    lv2_atom_sequence_clear(out);

    if (!self->preset_changed || self->midi_channel == NULL || self->midi_changes_only == NULL) {
        return;
    }

    // The port counts channels from 1, as the musician does; the message from 0
    const uint8_t channel = Controller_read_midi_value(self->midi_channel, 16) > 0
        ? Controller_read_midi_value(self->midi_channel, 16) - 1
        : 0;
    const bool changes_only = !is_zero(*self->midi_changes_only);
    float* const* dips = self->preset_outputs[self->current_preset];

    for (unsigned int output = 0; output < TOTAL_OUTPUTS; output++) {
        if (self->midi_cc[output] == NULL) {
            continue;
        }

        const uint8_t value = is_zero(*dips[output]) ? 0 : 127;

        if (changes_only && value == self->midi_last_sent[output]) {
            continue;
        }

        struct {
            LV2_Atom_Event event;
            uint8_t message[3];
        } cc;

        memset(&cc, 0, sizeof(cc));
        cc.event.time.frames = 0;
        cc.event.body.type = self->lv2->uris.midi_MidiEvent;
        cc.event.body.size = sizeof(cc.message);
        cc.message[0] = (uint8_t) (0xB0 | channel);
        cc.message[1] = Controller_read_midi_value(self->midi_cc[output], 127);
        cc.message[2] = value;

        // No room: the rest is dropped, and not taken as sent, so "changes only" sends it with the next change
        if (lv2_atom_sequence_append_event(out, capacity, &cc.event) != NULL) {
            self->midi_last_sent[output] = value;
        }
    }
}

static void Controller_set_default_preset_label(Controller* self, unsigned int index) {
    format_default_label(self->preset_labels[index], PRESET_LABEL_MAX_SIZE, "Preset", index);
}
