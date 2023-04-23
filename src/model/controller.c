#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <lv2/lv2plug.in/ns/ext/log/log.h>

#include "controller.h"
#include "../utils/utils.h"


bool Controller_is_preset_changed(void* self);

unsigned int Controller_get_index_previous_preset(void* self);
unsigned int Controller_get_index_current_preset(void* self);

unsigned int Controller_get_current_preset(void* self);

unsigned int Controller_get_output_signal(void* self);

// char* Controller_get_preset_label(void* self, unsigned int index);
// char* Controller_set_preset_label(void* self, unsigned int index, char* new_label);

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
    Controller* self = (Controller*) malloc(sizeof(Controller));

    self->is_preset_changed = &Controller_is_preset_changed;

    self->get_index_previous_preset = &Controller_get_index_previous_preset;
    self->get_index_current_preset = &Controller_get_index_current_preset;
    
    self->get_current_preset = &Controller_get_current_preset;

    self->get_output_signal = &Controller_get_output_signal;

    // self->get_preset_label = &Controller_get_preset_label;
    // self->set_preset_label = &Controller_set_preset_label;

    self->run = &Controller_run;
    
    self->internal_state.current_preset_mask = 0b0000001;
    self->internal_state.previous_preset_mask = self->internal_state.current_preset_mask;
    self->internal_state.preset_changed = false;

    self->lv2 = NULL;

    return self;
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

// char* Controller_get_preset_label(void* self, unsigned int index) {
//     Controller* this = (Controller*) self;

//     if (index >= TOTAL_PRESETS) {
//         return NULL;
//     }

//     return this->state.presets_label[index];
// }

// char* Controller_set_preset_label(void* self, unsigned int index, char* new_label) {
//     Controller* this = (Controller*) self;

//     if (index >= TOTAL_PRESETS) {
//         return NULL;
//     }

//     strcpy(this->state.presets_label[index], new_label);
    
//     return this->state.presets_label[index];
// }

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