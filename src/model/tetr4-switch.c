#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <lv2/lv2plug.in/ns/ext/log/log.h>

#include "../extension/lv2-hmi.h"

#include "tetr4-switch.h"
#include "../utils/utils.h"


void Tetr4Switch_run(void* self);

unsigned int Tetr4Switch_get_index_previous_preset(void* self);
unsigned int Tetr4Switch_get_index_current_preset(void* self);
unsigned int Tetr4Switch_get_current_preset(void* self);
unsigned int Tetr4Switch_get_inverter(void* self);
unsigned int Tetr4Switch_get_output_signal(void* self);

char* Tetr4Switch_get_preset_label(void* self, unsigned int index);
char* Tetr4Switch_set_preset_label(void* self, unsigned int index, char* new_label);

void Tetr4Switch_set_index_current_preset_by_mask(void* self, unsigned int index);
void Tetr4Switch_set_index_previous_preset_by_mask(void* self, unsigned int index);

/**
 * Calculate the preset index by mask.
 * If there more than one bit actived, is considered
 * the highest significative bit as the current preset.
 */
unsigned int preset_index_by_mask(unsigned int mask) {
    return highest_on_bit(mask);
}


void Tetr4Switch_instantiate(Tetr4Switch* self) {
    self->run = &Tetr4Switch_run;

    self->get_index_current_preset = &Tetr4Switch_get_index_current_preset;

    self->get_index_previous_preset = &Tetr4Switch_get_index_previous_preset;
    self->get_current_preset = &Tetr4Switch_get_current_preset;
    self->get_inverter = &Tetr4Switch_get_inverter;

    self->get_output_signal = &Tetr4Switch_get_output_signal;

    self->get_preset_label = &Tetr4Switch_get_preset_label;
    self->set_preset_label = &Tetr4Switch_set_preset_label;

    for (unsigned int i = 0; i<TOTAL_PRESETS; i++) {
        char message[10];
        sprintf(message, "Prst %d", i+1);

        self->preset_labels[i] = message;
    }

    self->current_preset_mask = 0b0000001;
    self->previous_preset_mask = self->current_preset_mask;
    self->preset_changed = false;
}

void Tetr4Switch_run(void* self) {
    Tetr4Switch* this = (Tetr4Switch*) self;

    unsigned int preset_mask = make_mask(this->preset_selectors, TOTAL_PRESETS);

    this->preset_changed = preset_mask != this->current_preset_mask;
    if (this->preset_changed) {
        Tetr4Switch_set_index_previous_preset_by_mask(this, this->current_preset_mask);

        unsigned int new_mask_current_preset = preset_mask ^ this->current_preset_mask;
        Tetr4Switch_set_index_current_preset_by_mask(this, new_mask_current_preset);
    }
}

void Tetr4Switch_set_index_previous_preset_by_mask(void* self, unsigned int preset_mask) {
    Tetr4Switch* this = (Tetr4Switch*) self;

    int index = preset_index_by_mask(preset_mask);
    this->previous_preset_mask = 0b1 << index;
}

unsigned int Tetr4Switch_get_index_previous_preset(void* self) {
    Tetr4Switch* this = (Tetr4Switch*) self;

    return preset_index_by_mask(this->previous_preset_mask);
}

void Tetr4Switch_set_index_current_preset_by_mask(void* self, unsigned int preset_mask) {
    Tetr4Switch* this = (Tetr4Switch*) self;

    int index = preset_index_by_mask(preset_mask);
    this->current_preset_mask = 0b1 << index;
}

unsigned int Tetr4Switch_get_index_current_preset(void* self) {
    Tetr4Switch* this = (Tetr4Switch*) self;

    return preset_index_by_mask(this->current_preset_mask);
}

unsigned int Tetr4Switch_get_current_preset(void* self) {
    Tetr4Switch* this = (Tetr4Switch*) self;

    unsigned int index_current_preset = this->get_index_current_preset(self);

    return make_mask(this->preset_outputs[index_current_preset], TOTAL_OUTPUTS);
}

unsigned int Tetr4Switch_get_inverter(void* self) {
    Tetr4Switch* this = (Tetr4Switch*) self;

    return make_mask(this->inverters, TOTAL_OUTPUTS);
}

unsigned int Tetr4Switch_get_output_signal(void* self) {
    Tetr4Switch* this = (Tetr4Switch*) self;

    return this->get_current_preset(this) ^ this->get_inverter(this);
}

char* Tetr4Switch_get_preset_label(void* self, unsigned int index) {
    Tetr4Switch* this = (Tetr4Switch*) self;

    if (index >= TOTAL_PRESETS) {
        return NULL;
    }

    return this->state.presets_label[index];
}

char* Tetr4Switch_set_preset_label(void* self, unsigned int index, char* new_label) {
    Tetr4Switch* this = (Tetr4Switch*) self;

    if (index >= TOTAL_PRESETS) {
        return NULL;
    }

    strcpy(this->state.presets_label[index], new_label);
    
    return this->state.presets_label[index];
}