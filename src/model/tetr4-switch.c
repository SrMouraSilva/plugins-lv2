#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "lv2/log/logger.h"

#include "../lv2-hmi.h"

#include "tetr4-switch.h"
#include "../utils/utils.h"


unsigned int Tetr4Switch_get_index_current_preset(void* self);
unsigned int Tetr4Switch_get_current_preset(void* self);
unsigned int Tetr4Switch_get_inverter(void* self);
unsigned int Tetr4Switch_get_output_signal(void* self);

char* Tetr4Switch_get_preset_label(void* self, unsigned int index);
char* Tetr4Switch_set_preset_label(void* self, unsigned int index, char* new_label);


void Tetr4Switch_instantiate(Tetr4Switch* self) {
    self->get_index_current_preset = &Tetr4Switch_get_index_current_preset;
    self->get_current_preset = &Tetr4Switch_get_current_preset;
    self->get_inverter = &Tetr4Switch_get_inverter;

    self->get_output_signal = &Tetr4Switch_get_output_signal;

    self->get_preset_label = &Tetr4Switch_get_preset_label;
    self->set_preset_label = &Tetr4Switch_set_preset_label;
}


unsigned int Tetr4Switch_get_index_current_preset(void* self) {
    Tetr4Switch* this = (Tetr4Switch*) self;

    const unsigned int preset_mask = make_mask(this->preset_selectors, TOTAL_PRESETS);
    return highest_on_bit(preset_mask);
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