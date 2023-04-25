#include <stdlib.h>
#include <stdbool.h>

#include <lv2/lv2plug.in/ns/lv2core/lv2.h>

#include "gossiper.h"
#include "../utils/utils.h"



void Gossiper_get_switches(void* self);

void Gossiper_run(void* self, uint32_t n_samples);

Gossiper* Gossiper_instantiate() {
    Gossiper* self = (Gossiper*) malloc(sizeof(Gossiper));

    self->run = &Gossiper_run;

    for (unsigned int i=0; i<TOTAL_GOSSIPER_FOOTSWITCHES; i++) {
        self->switches[i].activated = false;
        self->switches[i].updated = false;
    }

    for (unsigned int i=0; i<TOTAL_GOSSIPER_NOTIFIERS; i++) {
        self->notifiers[i].hmi_addressing = NULL;
    }

    // self->internal_state.get_preset_label = &Controller_get_preset_label;
    // self->internal_state.set_preset_label = &Controller_set_preset_label;

    //self->lv2 = NULL;

    return self;
}


void Gossiper_update_switches(Gossiper* this);
void Gossiper_update_output_cvs(Gossiper* this, uint32_t n_samples);

void Gossiper_run(void* self, uint32_t n_samples) {
    Gossiper* this = (Gossiper*) self;

    Gossiper_update_switches(this);
    Gossiper_update_output_cvs(this, n_samples);
}

void Gossiper_update_switches(Gossiper* this) {
    for (unsigned int i=0; i<TOTAL_GOSSIPER_FOOTSWITCHES; i++) {
        bool current_state = this->switches[i].activated;
        bool new_state = !is_zero(*this->switches->input);

        this->switches[i].updated = current_state == new_state;
        this->switches[i].activated = new_state;
    }
}

void Gossiper_update_output_cvs(Gossiper* this, uint32_t n_samples) {
    for (unsigned int output = 0; output < TOTAL_GOSSIPER_FOOTSWITCHES; output++) {
        float new_value = this->switches[output].activated * MAX_TENSION;

        for (uint32_t sample = 0; sample < n_samples; sample++) {
            this->outputs[output].output[sample] = new_value;
        }
    }
}