#include <stdlib.h>
#include <stdbool.h>

#include <lv2/lv2plug.in/ns/ext/log/log.h>

#include "controller.h"
#include "../utils/utils.h"

#define PRESET_LABEL_MAX_LENGTH (PRESET_LABEL_MAX_SIZE - 1)

static char* Controller_make_default_preset_label(unsigned int index);

bool Controller_is_preset_changed(void* self);

unsigned int Controller_get_index_previous_preset(void* self);
unsigned int Controller_get_index_current_preset(void* self);

unsigned int Controller_get_current_preset(void* self);

unsigned int Controller_get_output_signal(void* self);

void Controller_set_index_current_preset_by_mask(void* self, unsigned int index);
void Controller_set_index_previous_preset_by_mask(void* self, unsigned int index);

void Controller_run(void* self, uint32_t n_samples);

/**
 * Calculate the preset index by mask.
 * If there more than one bit actived, is considered
 * the highest significative bit as the current preset.
 */
unsigned int preset_index_by_mask(unsigned int mask) {
    return highest_on_bit(mask);
}


Controller* Controller_instantiate() {
    Controller* self = (Controller*) calloc(1, sizeof(Controller));

    if (self == NULL) {
        return NULL;
    }

    self->is_preset_changed = &Controller_is_preset_changed;

    self->get_index_previous_preset = &Controller_get_index_previous_preset;
    self->get_index_current_preset = &Controller_get_index_current_preset;

    self->get_current_preset = &Controller_get_current_preset;

    self->get_output_signal = &Controller_get_output_signal;

    self->run = &Controller_run;

    self->internal_state.current_preset_mask = 0b0000001;
    self->internal_state.previous_preset_mask = self->internal_state.current_preset_mask;
    self->internal_state.preset_changed = false;

    for (unsigned int i=0; i<TOTAL_PRESETS; i++) {
        self->preset_labels[i] = Controller_make_default_preset_label(i);

        if (self->preset_labels[i] == NULL) {
            Controller_cleanup(self);
            return NULL;
        }
    }

    self->lv2 = NULL;

    return self;
}

void Controller_cleanup(Controller* self) {
    if (self == NULL) {
        return;
    }

    for (unsigned int i=0; i<TOTAL_PRESETS; i++) {
        free(self->preset_labels[i]);
        self->preset_labels[i] = NULL;
    }
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

    char* next_label = (new_label == NULL || new_label[0] == '\0')
        ? Controller_make_default_preset_label(index)
        : sanitize_label(new_label, PRESET_LABEL_MAX_LENGTH);

    if (next_label == NULL) {
        return false;
    }

    free(self->preset_labels[index]);
    self->preset_labels[index] = next_label;

    return true;
}


bool Controller_is_preset_changed(void* self) {
    Controller* this = (Controller*) self;

    return this->internal_state.preset_changed;
}

void Controller_set_index_previous_preset_by_mask(void* self, unsigned int preset_mask) {
    Controller* this = (Controller*) self;

    int index = preset_index_by_mask(preset_mask);
    this->internal_state.previous_preset_mask = 0b1 << index;
}

unsigned int Controller_get_index_previous_preset(void* self) {
    Controller* this = (Controller*) self;

    return preset_index_by_mask(this->internal_state.previous_preset_mask);
}

void Controller_set_index_current_preset_by_mask(void* self, unsigned int preset_mask) {
    Controller* this = (Controller*) self;

    int index = preset_index_by_mask(preset_mask);
    this->internal_state.current_preset_mask = 0b1 << index;
}

unsigned int Controller_get_index_current_preset(void* self) {
    Controller* this = (Controller*) self;

    return preset_index_by_mask(this->internal_state.current_preset_mask);
}

unsigned int Controller_get_current_preset(void* self) {
    Controller* this = (Controller*) self;

    unsigned int index_current_preset = this->get_index_current_preset(self);

    return make_mask(this->preset_outputs[index_current_preset], TOTAL_OUTPUTS);
}

unsigned int Controller_get_output_signal(void* self) {
    Controller* this = (Controller*) self;

    return this->get_current_preset(this);
}

///////////////////////////////////////////////////////
// UPDATES
void Controller_update_assignables(Controller* self);
void Controller_update_output_cvs(Controller* self, uint32_t n_samples);


void Controller_run(void* self, uint32_t n_samples) {
    Controller* this = (Controller*) self;

    unsigned int preset_mask = make_mask(this->preset_selectors, TOTAL_PRESETS);
    unsigned int current_preset = (int) *this->current_preset_index;

    bool slider_changed = preset_mask != this->internal_state.current_preset_mask;
    bool combobox_changed = this->get_index_current_preset(this) != current_preset;

    this->internal_state.preset_changed = slider_changed || combobox_changed;

    if (this->internal_state.preset_changed) {
        Controller_set_index_previous_preset_by_mask(this, this->internal_state.current_preset_mask);

        unsigned int new_mask_current_preset = slider_changed
            ? (preset_mask ^ this->internal_state.current_preset_mask)
            : (new_mask_current_preset = 0b1 << current_preset);

        Controller_set_index_current_preset_by_mask(this, new_mask_current_preset);
    }

    Controller_update_assignables(self);
    Controller_update_output_cvs(self, n_samples);
}

void Controller_update_assignables(Controller* self) {
    unsigned int index = self->get_index_current_preset(self);

    if (self->internal_state.preset_changed) {
        unsigned int previous_index = self->get_index_previous_preset(self);

        // Footswitches
        *self->preset_selectors[previous_index] = 0.0f;
        *self->preset_selectors[index] = 1.0f;

        // Select
        *self->current_preset_index = index;
    }
}

void Controller_update_output_cvs(Controller* self, uint32_t n_samples) {
    // Calculate output values
    unsigned int output_coded = self->get_output_signal(self);
    float output_cv_values[TOTAL_OUTPUTS];

    for (unsigned int n = 0; n < TOTAL_OUTPUTS; n++) {
        unsigned int mask = 1 << n;
        output_cv_values[n] = ((output_coded & mask) >> n) * MAX_TENSION;
    }

    // Update CV values
    for (uint32_t i = 0; i < n_samples; i++) {
        for (unsigned int id_output = 0; id_output < TOTAL_OUTPUTS; id_output++) {
            self->output_cvs[id_output][i] = output_cv_values[id_output];
        }
    }
}

static char* Controller_make_default_preset_label(unsigned int index) {
    return make_default_label("Preset", index);
}