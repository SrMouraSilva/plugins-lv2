#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#include <lv2/lv2plug.in/ns/ext/atom/util.h>
#include <lv2/lv2plug.in/ns/ext/patch/patch.h>
#include <lv2/lv2plug.in/ns/ext/state/state.h>

#include "model/gossiper.h"
#include "lv2/gossiper/lv2-gossiper.h"
#include "lv2/gossiper/lv2-hmi-gossiper.h"

static void Gossiper_apply_patch_messages(Gossiper* self);
static void Gossiper_apply_patch_set(Gossiper* self, const LV2_Atom_Object* obj);
static void Gossiper_reply_patch_get(Gossiper* self, const LV2_Atom_Object* obj, int64_t frames);
static void Gossiper_send_patch_set(Gossiper* self, int64_t frames, LV2_URID property_urid, const char* label);
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
    Gossiper* self = Gossiper_instantiate();

    if (self == NULL) {
        return NULL;
    }

    self->lv2 = LV2_Gossiper_instantiate();

    if (self->lv2 == NULL) {
        Gossiper_cleanup(self);
        free(self);
        return NULL;
    }

    const char* missing = self->lv2->initialize(self->lv2, features);

    if (missing) {
        free(self->lv2);
        Gossiper_cleanup(self);
        free(self);
        return NULL;
    }

    // Atom_instantiate(self);

    return (LV2_Handle) self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    Gossiper* self = (Gossiper*) instance;

    switch ((PortIndex) port) {
        case OUTPUT_CV_1:
            self->switches[0].output = (float*) data; break;
        case OUTPUT_CV_2:
            self->switches[1].output = (float*) data; break;
        case OUTPUT_CV_3:
            self->switches[2].output = (float*) data; break;
        case OUTPUT_CV_4:
            self->switches[3].output = (float*) data; break;
        case OUTPUT_CV_5:
            self->switches[4].output = (float*) data; break;
        case OUTPUT_CV_6:
            self->switches[5].output = (float*) data; break;
        case OUTPUT_CV_7:
            self->switches[6].output = (float*) data; break;
        
        case FOOTSWITCH_1:
            self->switches[0].input = (float*) data; break;
        case FOOTSWITCH_2:
            self->switches[1].input = (float*) data; break;
        case FOOTSWITCH_3:
            self->switches[2].input = (float*) data; break;
        case FOOTSWITCH_4:
            self->switches[3].input = (float*) data; break;
        case FOOTSWITCH_5:
            self->switches[4].input = (float*) data; break;
        case FOOTSWITCH_6:
            self->switches[5].input = (float*) data; break;
        case FOOTSWITCH_7:
            self->switches[6].input = (float*) data; break;

        case POTENTIOMETER_1:
            self->potentiometers[0].input = (float*) data; break;
        case MINIMUM_1:
            self->potentiometers[0].minimum = (float*) data; break;
        case MAXIMUM_1:
            self->potentiometers[0].maximum = (float*) data; break;
        case OUTPUT_POTENTIOMETER_CV_1:
            self->potentiometers[0].output = (float*) data; break;

        case POTENTIOMETER_2:
            self->potentiometers[1].input = (float*) data; break;
        case MINIMUM_2:
            self->potentiometers[1].minimum = (float*) data; break;
        case MAXIMUM_2:
            self->potentiometers[1].maximum = (float*) data; break;
        case OUTPUT_POTENTIOMETER_CV_2:
            self->potentiometers[1].output = (float*) data; break;

        case POTENTIOMETER_3:
            self->potentiometers[2].input = (float*) data; break;
        case MINIMUM_3:
            self->potentiometers[2].minimum = (float*) data; break;
        case MAXIMUM_3:
            self->potentiometers[2].maximum = (float*) data; break;
        case OUTPUT_POTENTIOMETER_CV_3:
            self->potentiometers[2].output = (float*) data; break;

        case POTENTIOMETER_4:
            self->potentiometers[3].input = (float*) data; break;
        case MINIMUM_4:
            self->potentiometers[3].minimum = (float*) data; break;
        case MAXIMUM_4:
            self->potentiometers[3].maximum = (float*) data; break;
        case OUTPUT_POTENTIOMETER_CV_4:
            self->potentiometers[3].output = (float*) data; break;

        case ASSIGN_TO_NOTIFY_1:
        case ASSIGN_TO_NOTIFY_2:
        case ASSIGN_TO_NOTIFY_3:
        case ASSIGN_TO_NOTIFY_4:
            break;

        case EVENTS_IN:
            self->lv2->events_in = (const LV2_Atom_Sequence*) data; break;
        case EVENTS_OUT:
            self->lv2->events_out = (LV2_Atom_Sequence*) data; break;
    }
}

static void activate(LV2_Handle instance) {}


static void Gossiper_start_atom_forge(Gossiper* self, LV2_Atom_Forge_Frame* notify_frame) {
    LV2_Atom_Sequence* out = self->lv2->events_out;

    if (out == NULL) {
        return;
    }

    const uint32_t out_capacity = out->atom.size;

    lv2_atom_forge_set_buffer(&self->lv2->forge, (uint8_t*) out, out_capacity);
    lv2_atom_forge_sequence_head(&self->lv2->forge, notify_frame, 0);
}

static void Gossiper_finish_atom_forge(Gossiper* self, LV2_Atom_Forge_Frame* notify_frame) {
    if (self->lv2->events_out == NULL) {
        return;
    }

    lv2_atom_forge_pop(&self->lv2->forge, notify_frame);
}

static void run(LV2_Handle instance, uint32_t n_samples) {
    Gossiper* self = (Gossiper*) instance;

    LV2_Atom_Forge_Frame notify_frame;
    Gossiper_start_atom_forge(self, &notify_frame);

    Gossiper_apply_patch_messages(self);

    self->run(self, n_samples);

    LV2_HMI_Gossiper_run(self);
    //Atom_run(self);

    Gossiper_finish_atom_forge(self, &notify_frame);
}


static void deactivate(LV2_Handle instance) {}

static void cleanup(LV2_Handle instance) {
    Gossiper* self = (Gossiper*) instance;

    free(self->lv2);
    self->lv2 = NULL;

    Gossiper_cleanup(self);
    free(self);
}

static const void* extension_data(const char* uri) {
    if (LV2_HMI_Gossiper_is_extension_data_appliable(uri)) {
        return LV2_HMI_Gossiper_extension_data();
    }

    if (!strcmp(uri, LV2_STATE__interface)) {
        return &state;
    }
    
    return NULL;
}

static void Gossiper_apply_patch_messages(Gossiper* self) {
    if (self->lv2->events_in == NULL) {
        return;
    }

    LV2_ATOM_SEQUENCE_FOREACH(self->lv2->events_in, ev) {
        if (ev->body.type != self->lv2->uris.atom_Object) {
            continue;
        }

        const LV2_Atom_Object* obj = (const LV2_Atom_Object*) &ev->body;
        if (obj->body.otype == self->lv2->uris.patch_Set) {
            Gossiper_apply_patch_set(self, obj);
        } else if (obj->body.otype == self->lv2->uris.patch_Get) {
            Gossiper_reply_patch_get(self, obj, ev->time.frames);
        }
    }
}

static void Gossiper_apply_patch_set(Gossiper* self, const LV2_Atom_Object* obj) {
    const LV2_Atom* property = NULL;
    const LV2_Atom* value = NULL;

    lv2_atom_object_get(
        obj,
        self->lv2->uris.patch_property, &property,
        self->lv2->uris.patch_value, &value,
        0
    );

    if (property == NULL || property->type != self->lv2->uris.atom_URID) {
        lv2_log_error(&self->lv2->logger, "Gossiper: malformed patch:Set property\n");
        return;
    }

    if (value == NULL || value->type != self->lv2->uris.atom_String) {
        lv2_log_error(&self->lv2->logger, "Gossiper: invalid footswitch label value\n");
        return;
    }

    LV2_URID property_urid = ((const LV2_Atom_URID*) property)->body;
    const char* label = (const char*) (value + 1);

    for (unsigned int i=0; i<TOTAL_GOSSIPER_FOOTSWITCHES; i++) {
        if (self->lv2->uris.footswitch_label[i] != property_urid) {
            continue;
        }

        if (!Gossiper_set_footswitch_label(self, i, label)) {
            lv2_log_error(&self->lv2->logger, "Gossiper: could not store footswitch label\n");
        }

        return;
    }

    lv2_log_error(&self->lv2->logger, "Gossiper: unknown patch property\n");
}

static void Gossiper_reply_patch_get(Gossiper* self, const LV2_Atom_Object* obj, int64_t frames) {
    if (self->lv2->events_out == NULL) {
        return;
    }

    const LV2_Atom* property = NULL;

    lv2_atom_object_get(
        obj,
        self->lv2->uris.patch_property, &property,
        0
    );

    // No property specified: reply with every footswitch label
    if (property == NULL) {
        for (unsigned int i=0; i<TOTAL_GOSSIPER_FOOTSWITCHES; i++) {
            Gossiper_send_patch_set(self, frames, self->lv2->uris.footswitch_label[i], Gossiper_get_footswitch_label(self, i));
        }
        return;
    }

    if (property->type != self->lv2->uris.atom_URID) {
        lv2_log_error(&self->lv2->logger, "Gossiper: malformed patch:Get property\n");
        return;
    }

    LV2_URID property_urid = ((const LV2_Atom_URID*) property)->body;

    for (unsigned int i=0; i<TOTAL_GOSSIPER_FOOTSWITCHES; i++) {
        if (self->lv2->uris.footswitch_label[i] != property_urid) {
            continue;
        }

        Gossiper_send_patch_set(self, frames, property_urid, Gossiper_get_footswitch_label(self, i));
        return;
    }

    lv2_log_error(&self->lv2->logger, "Gossiper: unknown patch:Get property\n");
}

static void Gossiper_send_patch_set(Gossiper* self, int64_t frames, LV2_URID property_urid, const char* label) {
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
    Gossiper* self = (Gossiper*) instance;

    for (unsigned int i=0; i<TOTAL_GOSSIPER_FOOTSWITCHES; i++) {
        const char* label = Gossiper_get_footswitch_label(self, i);

        if (label == NULL) {
            continue;
        }

        store(
            handle,
            self->lv2->uris.footswitch_label[i],
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
    Gossiper* self = (Gossiper*) instance;

    for (unsigned int i=0; i<TOTAL_GOSSIPER_FOOTSWITCHES; i++) {
        size_t size = 0;
        uint32_t type = 0;
        uint32_t value_flags = 0;
        const void* value = retrieve(
            handle,
            self->lv2->uris.footswitch_label[i],
            &size,
            &type,
            &value_flags
        );

        if (value == NULL) {
            continue;
        }

        if (type != self->lv2->uris.atom_String) {
            lv2_log_error(&self->lv2->logger, "Gossiper: invalid stored footswitch label type\n");
            continue;
        }

        if (!Gossiper_set_footswitch_label(self, i, (const char*) value)) {
            lv2_log_error(&self->lv2->logger, "Gossiper: could not restore footswitch label\n");
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
