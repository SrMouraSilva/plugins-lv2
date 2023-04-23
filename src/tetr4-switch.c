#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#include "model/tetr4-switch.h"
#include "model/controller.h"

#include "lv2/lv2-controller.h"
#include "lv2/lv2-hmi-extension.h"
#include "lv2/atom-extension.h"


static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
    Controller* self = Controller_instantiate();

    self->lv2 = LV2_Controller_instantiate();

    const char* missing = self->lv2->initialize(self->lv2, features);

    if (missing) {
        //cleanup(self);
        free(self);
        return NULL;
    }

    // Atom_instantiate(self);

    return (LV2_Handle) self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    Controller* self = (Controller*) instance;

    switch ((PortIndex) port) {
        case OUTPUT_CV_1:
            self->output_cvs[0] = (float*) data; break;
        case OUTPUT_CV_2:
            self->output_cvs[1] = (float*) data; break;
        case OUTPUT_CV_3:
            self->output_cvs[2] = (float*) data; break;
        case OUTPUT_CV_4:
            self->output_cvs[3] = (float*) data; break;
        
        case PRESET_SELECTOR_1:
            self->preset_selectors[0] = (float*) data; break;
        case PRESET_SELECTOR_2:
            self->preset_selectors[1] = (float*) data; break;
        case PRESET_SELECTOR_3:
            self->preset_selectors[2] = (float*) data; break;
        case PRESET_SELECTOR_4:
            self->preset_selectors[3] = (float*) data; break;

        case PRESET_SELECT:
            self->current_preset_index = (float*) data; break;

        case ASSIGN_TO_NOTIFY:
            break;

        case PRESET_1_OUTPUT_1:
            self->preset_outputs[0][0] = (float*) data; break;
        case PRESET_1_OUTPUT_2:
            self->preset_outputs[0][1] = (float*) data; break;
        case PRESET_1_OUTPUT_3:
            self->preset_outputs[0][2] = (float*) data; break;
        case PRESET_1_OUTPUT_4:
            self->preset_outputs[0][3] = (float*) data; break;

        case PRESET_2_OUTPUT_1:
            self->preset_outputs[1][0] = (float*) data; break;
        case PRESET_2_OUTPUT_2:
            self->preset_outputs[1][1] = (float*) data; break;
        case PRESET_2_OUTPUT_3:
            self->preset_outputs[1][2] = (float*) data; break;
        case PRESET_2_OUTPUT_4:
            self->preset_outputs[1][3] = (float*) data; break;

        case PRESET_3_OUTPUT_1:
            self->preset_outputs[2][0] = (float*) data; break;
        case PRESET_3_OUTPUT_2:
            self->preset_outputs[2][1] = (float*) data; break;
        case PRESET_3_OUTPUT_3:
            self->preset_outputs[2][2] = (float*) data; break;
        case PRESET_3_OUTPUT_4:
            self->preset_outputs[2][3] = (float*) data; break;

        case PRESET_4_OUTPUT_1:
            self->preset_outputs[3][0] = (float*) data; break;
        case PRESET_4_OUTPUT_2:
            self->preset_outputs[3][1] = (float*) data; break;
        case PRESET_4_OUTPUT_3:
            self->preset_outputs[3][2] = (float*) data; break;
        case PRESET_4_OUTPUT_4:
            self->preset_outputs[3][3] = (float*) data; break;

        case EVENTS_IN:
            self->lv2->atom.events_in = (const LV2_Atom_Sequence*) data; break;
    }
}

static void activate(LV2_Handle instance) {}


void update_assignables(Controller* self);

static void run(LV2_Handle instance, uint32_t n_samples) {
    Controller* self = (Controller*) instance;

    self->run(self, n_samples);

    LV2_HMI_run(self);
    //Atom_run(self);
    
    update_assignables(self);
}

void update_assignables(Controller* self) {
    unsigned int index = self->get_index_current_preset(self);

    if (self->is_preset_changed(self)) {
        unsigned int previous_index = self->get_index_previous_preset(self);

        LV2_ControlInputPort_Change_Request* control_input_port = self->lv2->control_input_port;

        control_input_port->request_change(control_input_port->handle, PRESET_SELECTOR_1 + previous_index, 0.0f);
        control_input_port->request_change(control_input_port->handle, PRESET_SELECTOR_1 + index, 1.0f);

        control_input_port->request_change(control_input_port->handle, PRESET_SELECT, index);
    }
}


static void deactivate(LV2_Handle instance) {}

static void cleanup(LV2_Handle instance) {
    Controller* self = (Controller*) instance;

    free(self->lv2);
    self->lv2 = NULL;

    free(self);
}

static const void* extension_data(const char* uri) {
    if (LV2_HMI_is_extension_data_appliable(uri)) {
        return LV2_HMI_extension_data();
    }
    
    return NULL;
}

static const LV2_Descriptor descriptor = {
    PLUGIN_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return index == 0
         ? &descriptor
         : NULL;
}
