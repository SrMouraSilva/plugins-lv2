#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>

#include <lv2/lv2plug.in/ns/lv2core/lv2.h>

#include "gossiper.h"
#include "../utils/utils.h"



#define FOOTSWITCH_LABEL_MAX_LENGTH 15

void Gossiper_get_switches(void* self);

static char* Gossiper_make_default_footswitch_label(unsigned int index);

void Gossiper_run(void* self, uint32_t n_samples);

Gossiper* Gossiper_instantiate() {
    Gossiper* self = (Gossiper*) calloc(1, sizeof(Gossiper));

    if (self == NULL) {
        return NULL;
    }

    self->run = &Gossiper_run;

    for (unsigned int i=0; i<TOTAL_GOSSIPER_FOOTSWITCHES; i++) {
        self->switches[i].activated = false;
        self->switches[i].updated = false;
    }

    for (unsigned int i=0; i<TOTAL_GOSSIPER_POTENTIOMETER; i++) {
        self->potentiometers[i].updated = false;
        self->potentiometers[i].value = 0.0f;
        self->potentiometers[i].delay = 0;
    }

    for (unsigned int i=0; i<TOTAL_GOSSIPER_NOTIFIERS; i++) {
        self->notifiers[i].hmi_addressing = NULL;
    }

    for (unsigned int i=0; i<TOTAL_GOSSIPER_FOOTSWITCHES; i++) {
        self->footswitch_labels[i] = Gossiper_make_default_footswitch_label(i);

        if (self->footswitch_labels[i] == NULL) {
            Gossiper_cleanup(self);
            return NULL;
        }
    }

    // self->internal_state.get_preset_label = &Controller_get_preset_label;
    // self->internal_state.set_preset_label = &Controller_set_preset_label;

    self->lv2 = NULL;

    return self;
}

void Gossiper_cleanup(Gossiper* self) {
    if (self == NULL) {
        return;
    }

    for (unsigned int i=0; i<TOTAL_GOSSIPER_FOOTSWITCHES; i++) {
        free(self->footswitch_labels[i]);
        self->footswitch_labels[i] = NULL;
    }
}

const char* Gossiper_get_footswitch_label(const Gossiper* self, unsigned int index) {
    if (self == NULL || index >= TOTAL_GOSSIPER_FOOTSWITCHES) {
        return NULL;
    }

    return self->footswitch_labels[index];
}

bool Gossiper_set_footswitch_label(Gossiper* self, unsigned int index, const char* new_label) {
    if (self == NULL || index >= TOTAL_GOSSIPER_FOOTSWITCHES) {
        return false;
    }

    char* next_label = (new_label == NULL || new_label[0] == '\0')
        ? Gossiper_make_default_footswitch_label(index)
        : sanitize_label(new_label, FOOTSWITCH_LABEL_MAX_LENGTH);

    if (next_label == NULL) {
        return false;
    }

    free(self->footswitch_labels[index]);
    self->footswitch_labels[index] = next_label;

    return true;
}


void Gossiper_update_switches(Gossiper* this);
void Gossiper_update_potentiometers(Gossiper* this);
void Gossiper_update_output_cvs(Gossiper* this, uint32_t n_samples);

void Gossiper_run(void* self, uint32_t n_samples) {
    Gossiper* this = (Gossiper*) self;

    Gossiper_update_switches(this);
    Gossiper_update_potentiometers(this);
    Gossiper_update_output_cvs(this, n_samples);
}

void Gossiper_update_switches(Gossiper* this) {
    for (unsigned int i=0; i<TOTAL_GOSSIPER_FOOTSWITCHES; i++) {
        bool current_state = this->switches[i].activated;
        bool new_state = !is_zero(*this->switches[i].input);

        this->switches[i].updated = current_state != new_state;
        this->switches[i].activated = new_state;
    }
}

void Gossiper_update_potentiometers(Gossiper* this) {
    for (unsigned int i=0; i<TOTAL_GOSSIPER_POTENTIOMETER; i++) {
        float current_value = this->potentiometers[i].value;
        float new_value = *this->potentiometers[i].input;

        bool updated = current_value != new_value;

        // Finished count
        if (this->potentiometers[i].delay == 0 && !updated) {
            this->potentiometers[i].updated = false;

        // Start counting
        } else if (this->potentiometers[i].delay == 0 && updated) {
            this->potentiometers[i].delay = DELAY_TO_SHOW_A_MESSAGE;
            this->potentiometers[i].updated = true;

        // When counting, decrease the counting
        } else if (this->potentiometers[i].delay > 0) {
            this->potentiometers[i].delay -= 1;
            this->potentiometers[i].updated = true;
        }

        this->potentiometers[i].value = new_value;
    }
}

void Gossiper_update_output_cvs(Gossiper* this, uint32_t n_samples) {
    for (unsigned int output = 0; output < TOTAL_GOSSIPER_FOOTSWITCHES; output++) {
        float new_value = this->switches[output].activated * MAX_TENSION;

        for (uint32_t sample = 0; sample < n_samples; sample++) {
            this->switches[output].output[sample] = new_value;
        }
    }

    for (unsigned int output = 0; output < TOTAL_GOSSIPER_POTENTIOMETER; output++) {
        float min = *this->potentiometers[output].minimum;
        float max = *this->potentiometers[output].maximum;

        float current = this->potentiometers[output].value;
        float new_value = fmin(min, max) + (fmax(min, max) - fmin(min, max)) * current;

        for (uint32_t sample = 0; sample < n_samples; sample++) {
            this->potentiometers[output].output[sample] = new_value;
        }
    }
}

static char* Gossiper_make_default_footswitch_label(unsigned int index) {
    return make_default_label("Footswitch", index);
}