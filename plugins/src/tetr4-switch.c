#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#include <lv2/lv2plug.in/ns/ext/atom/util.h>
#include <lv2/lv2plug.in/ns/ext/patch/patch.h>
#include <lv2/lv2plug.in/ns/ext/state/state.h>

#include "model/tetr4-switch.h"
#include "model/controller.h"

#include "lv2/controller/lv2-controller.h"
#include "lv2/controller/lv2-hmi-extension.h"
#include "lv2/controller/atom-extension.h"

static void Controller_apply_patch_messages(Controller* self);
static void Controller_apply_patch_set(Controller* self, const LV2_Atom_Object* obj);
static void Controller_reply_patch_get(Controller* self, const LV2_Atom_Object* obj, int64_t frames);
static void Controller_send_patch_set(Controller* self, int64_t frames, LV2_URID property_urid, const char* label);
static LV2_State_Status save(LV2_Handle instance,
                             LV2_State_Store_Function store,
                             LV2_State_Handle handle,
                             uint32_t flags,
                             const LV2_Feature* const* features);
static LV2_State_Status restore(LV2_Handle instance,
                                LV2_State_Retrieve_Function retrieve,
                                LV2_State_Handle handle,
                                uint32_t flags,
                                const LV2_Feature* const* features);

static const LV2_State_Interface state = {
    save,
    restore,
};

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
    Controller* self = Controller_instantiate();

    if (self == NULL) {
        return NULL;
    }

    self->lv2 = LV2_Controller_instantiate();

    if (self->lv2 == NULL) {
        Controller_cleanup(self);
        free(self);
        return NULL;
    }

    const char* missing = self->lv2->initialize(self->lv2, features);

    if (missing) {
        free(self->lv2);
        Controller_cleanup(self);
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

        case ASSIGN_TO_NOTIFY_1:
        case ASSIGN_TO_NOTIFY_2:
        case ASSIGN_TO_NOTIFY_3:
        case ASSIGN_TO_NOTIFY_4:
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
            self->lv2->events_in = (const LV2_Atom_Sequence*) data; break;
        case EVENTS_OUT:
            self->lv2->events_out = (LV2_Atom_Sequence*) data; break;
    }
}

static void activate(LV2_Handle instance) {}


void update_assignables(Controller* self);

static void Controller_start_atom_forge(Controller* self, LV2_Atom_Forge_Frame* notify_frame) {
    LV2_Atom_Sequence* out = self->lv2->events_out;

    if (out == NULL) {
        return;
    }

    const uint32_t out_capacity = out->atom.size;

    lv2_atom_forge_set_buffer(&self->lv2->forge, (uint8_t*) out, out_capacity);
    lv2_atom_forge_sequence_head(&self->lv2->forge, notify_frame, 0);
}

static void Controller_finish_atom_forge(Controller* self, LV2_Atom_Forge_Frame* notify_frame) {
    if (self->lv2->events_out == NULL) {
        return;
    }

    lv2_atom_forge_pop(&self->lv2->forge, notify_frame);
}

static void run(LV2_Handle instance, uint32_t n_samples) {
    Controller* self = (Controller*) instance;

    LV2_Atom_Forge_Frame notify_frame;
    Controller_start_atom_forge(self, &notify_frame);

    Controller_apply_patch_messages(self);

    self->run(self, n_samples);

    LV2_HMI_run(self);
    //Atom_run(self);

    update_assignables(self);

    Controller_finish_atom_forge(self, &notify_frame);
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

    Controller_cleanup(self);
    free(self);
}

static const void* extension_data(const char* uri) {
    if (LV2_HMI_is_extension_data_appliable(uri)) {
        return LV2_HMI_extension_data();
    }

    if (!strcmp(uri, LV2_STATE__interface)) {
        return &state;
    }

    return NULL;
}

static void Controller_apply_patch_messages(Controller* self) {
    if (self->lv2->events_in == NULL) {
        return;
    }

    LV2_ATOM_SEQUENCE_FOREACH(self->lv2->events_in, ev) {
        if (ev->body.type != self->lv2->uris.atom_Object) {
            continue;
        }

        const LV2_Atom_Object* obj = (const LV2_Atom_Object*) &ev->body;
        if (obj->body.otype == self->lv2->uris.patch_Set) {
            Controller_apply_patch_set(self, obj);
        } else if (obj->body.otype == self->lv2->uris.patch_Get) {
            Controller_reply_patch_get(self, obj, ev->time.frames);
        }
    }
}

static void Controller_apply_patch_set(Controller* self, const LV2_Atom_Object* obj) {
    const LV2_Atom* property = NULL;
    const LV2_Atom* value = NULL;

    lv2_atom_object_get(
        obj,
        self->lv2->uris.patch_property, &property,
        self->lv2->uris.patch_value, &value,
        0
    );

    if (property == NULL || property->type != self->lv2->uris.atom_URID) {
        lv2_log_error(&self->lv2->logger, "Tetr4-switch: malformed patch:Set property\n");
        return;
    }

    if (value == NULL || value->type != self->lv2->uris.atom_String) {
        lv2_log_error(&self->lv2->logger, "Tetr4-switch: invalid preset label value\n");
        return;
    }

    LV2_URID property_urid = ((const LV2_Atom_URID*) property)->body;
    const char* label = (const char*) (value + 1);

    for (unsigned int i=0; i<TOTAL_PRESETS; i++) {
        if (self->lv2->uris.preset_label[i] != property_urid) {
            continue;
        }

        if (!Controller_set_preset_label(self, i, label)) {
            lv2_log_error(&self->lv2->logger, "Tetr4-switch: could not store preset label\n");
        }

        return;
    }

    lv2_log_error(&self->lv2->logger, "Tetr4-switch: unknown patch property\n");
}

static void Controller_reply_patch_get(Controller* self, const LV2_Atom_Object* obj, int64_t frames) {
    if (self->lv2->events_out == NULL) {
        return;
    }

    const LV2_Atom* property = NULL;

    lv2_atom_object_get(
        obj,
        self->lv2->uris.patch_property, &property,
        0
    );

    // No property specified: reply with every preset label
    if (property == NULL) {
        for (unsigned int i=0; i<TOTAL_PRESETS; i++) {
            Controller_send_patch_set(self, frames, self->lv2->uris.preset_label[i], Controller_get_preset_label(self, i));
        }
        return;
    }

    if (property->type != self->lv2->uris.atom_URID) {
        lv2_log_error(&self->lv2->logger, "Tetr4-switch: malformed patch:Get property\n");
        return;
    }

    LV2_URID property_urid = ((const LV2_Atom_URID*) property)->body;

    for (unsigned int i=0; i<TOTAL_PRESETS; i++) {
        if (self->lv2->uris.preset_label[i] != property_urid) {
            continue;
        }

        Controller_send_patch_set(self, frames, property_urid, Controller_get_preset_label(self, i));
        return;
    }

    lv2_log_error(&self->lv2->logger, "Tetr4-switch: unknown patch:Get property\n");
}

static void Controller_send_patch_set(Controller* self, int64_t frames, LV2_URID property_urid, const char* label) {
    if (label == NULL) {
        return;
    }

    LV2_Atom_Forge* forge = &self->lv2->forge;
    LV2_Atom_Forge_Frame obj_frame;

    lv2_atom_forge_frame_time(forge, frames);
    lv2_atom_forge_object(forge, &obj_frame, 0, self->lv2->uris.patch_Set);

    lv2_atom_forge_key(forge, self->lv2->uris.patch_property);
    lv2_atom_forge_urid(forge, property_urid);

    lv2_atom_forge_key(forge, self->lv2->uris.patch_value);
    lv2_atom_forge_string(forge, label, strlen(label));

    lv2_atom_forge_pop(forge, &obj_frame);
}

static LV2_State_Status save(LV2_Handle instance,
                             LV2_State_Store_Function store,
                             LV2_State_Handle handle,
                             uint32_t flags,
                             const LV2_Feature* const* features) {
    Controller* self = (Controller*) instance;

    for (unsigned int i=0; i<TOTAL_PRESETS; i++) {
        const char* label = Controller_get_preset_label(self, i);

        if (label == NULL) {
            continue;
        }

        store(
            handle,
            self->lv2->uris.preset_label[i],
            label,
            strlen(label) + 1,
            self->lv2->uris.atom_String,
            LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE
        );
    }

    return LV2_STATE_SUCCESS;
}

static LV2_State_Status restore(LV2_Handle instance,
                                LV2_State_Retrieve_Function retrieve,
                                LV2_State_Handle handle,
                                uint32_t flags,
                                const LV2_Feature* const* features) {
    Controller* self = (Controller*) instance;

    for (unsigned int i=0; i<TOTAL_PRESETS; i++) {
        size_t size = 0;
        uint32_t type = 0;
        uint32_t value_flags = 0;
        const void* value = retrieve(
            handle,
            self->lv2->uris.preset_label[i],
            &size,
            &type,
            &value_flags
        );

        if (value == NULL) {
            continue;
        }

        if (type != self->lv2->uris.atom_String) {
            lv2_log_error(&self->lv2->logger, "Tetr4-switch: invalid stored preset label type\n");
            continue;
        }

        if (!Controller_set_preset_label(self, i, (const char*) value)) {
            lv2_log_error(&self->lv2->logger, "Tetr4-switch: could not restore preset label\n");
        }
    }

    return LV2_STATE_SUCCESS;
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
