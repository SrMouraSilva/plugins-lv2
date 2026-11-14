/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#include <lv2/lv2plug.in/ns/ext/atom/util.h>
#include <lv2/lv2plug.in/ns/ext/patch/patch.h>
#include <lv2/lv2plug.in/ns/ext/state/state.h>

#include "model/ports.h"
#include "model/controller.h"

#include "lv2/controller/lv2-controller.h"
#include "lv2/controller/lv2-hmi-extension.h"

static void Controller_apply_patch_messages(Controller* self);
static void Controller_apply_patch_set(Controller* self, const LV2_Atom_Object* obj);
static void Controller_reply_patch_get(Controller* self, const LV2_Atom_Object* obj, int64_t frames);
static void Controller_send_patch_set(Controller* self, int64_t frames, LV2_URID property_urid, const char* label);
static void Controller_send_restored_labels(Controller* self);
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

static const LV2_Worker_Interface worker = {
    LV2_HMI_work,
    LV2_HMI_work_response,
    NULL,
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

    return (LV2_Handle) self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    Controller* self = (Controller*) instance;

    // The CV outputs are the first ports: nothing to compare against on the lower side (port is unsigned)
    if (port < PORT_PRESET_SELECTOR_FIRST) {
        self->output_cvs[port - PORT_OUTPUT_CV_FIRST] = (float*) data;

    } else if (port >= PORT_PRESET_SELECTOR_FIRST && port < PORT_PRESET_SELECT) {
        self->preset_selectors[port - PORT_PRESET_SELECTOR_FIRST] = (float*) data;

    } else if (port == PORT_PRESET_SELECT) {
        self->current_preset_index = (float*) data;

    } else if (port >= PORT_ASSIGN_TO_NOTIFY_FIRST && port < PORT_PRESET_OUTPUT_FIRST) {
        // Only addressed to the device screen: the plugin doesn't read them

    } else if (port >= PORT_PRESET_OUTPUT_FIRST && port < PORT_EVENTS_IN) {
        unsigned int dip = port - PORT_PRESET_OUTPUT_FIRST;
        self->preset_outputs[dip / TOTAL_OUTPUTS][dip % TOTAL_OUTPUTS] = (float*) data;

    } else if (port == PORT_EVENTS_IN) {
        self->lv2->events_in = (const LV2_Atom_Sequence*) data;

    } else if (port == PORT_EVENTS_OUT) {
        self->lv2->events_out = (LV2_Atom_Sequence*) data;

    } else if (port >= PORT_PRESET_IN_CYCLE_FIRST && port < PORT_PRESET_IN_CYCLE_FIRST + TOTAL_PRESETS) {
        // Hept7 Switch only: Tetr4 Switch has no such ports, so the host never connects them
        self->presets_in_cycle[port - PORT_PRESET_IN_CYCLE_FIRST] = (float*) data;

    } else if (port == PORT_ACTIVE_PRESET) {
        // Hept7 Switch only, as above
        self->active_preset_output = (float*) data;
    }
}

static void activate(LV2_Handle instance) {}


static void Controller_sync_selector_ports(Controller* self);

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

    // Frame 0, before any patch:Get reply, so the output events stay in time order
    Controller_send_restored_labels(self);

    Controller_apply_patch_messages(self);

    Controller_run(self, n_samples);

    LV2_HMI_run(self);

    Controller_sync_selector_ports(self);

    Controller_finish_atom_forge(self, &notify_frame);
}

/**
 * The selectors work as radio buttons and the combobox follows them: ask the host to set the ones that disagree with
 * the active preset (only those, and only when something moved). The plugin never writes into its input ports
 * itself: the host does it, and tells MOD UI.
 */
static void Controller_sync_selector_ports(Controller* self) {
    if (!self->preset_changed && !self->inputs_changed) {
        return;
    }

    LV2_ControlInputPort_Change_Request* control_input_port = self->lv2->control_input_port;
    const unsigned int current = self->current_preset;

    // What the host accepted is what the ports hold from now on (mod-host writes them at once): remember it as what
    // was read, so the next cycle does not take the plugin's own changes for a selector turned on or the combobox
    // moved, which would hide a real change arriving on that cycle
    for (unsigned int i=0; i<TOTAL_PRESETS; i++) {
        const bool on = i == current;
        const bool is_on = (self->selector_mask >> i) & 1u;

        if (on != is_on
            && control_input_port->request_change(control_input_port->handle, PORT_PRESET_SELECTOR_FIRST + i, on ? 1.0f : 0.0f) == LV2_CONTROL_INPUT_PORT_CHANGE_SUCCESS) {
            self->selector_mask ^= 1u << i;
        }
    }

    // With a cycle (Hept7 Switch) the combobox is also the device's position (see Controller_follow_combobox). It never
    // takes a value near that position: the next press of the device would send it (the host would see no change, and
    // the press would be lost), or it would make a press ambiguous. It keeps its value instead, and the selectors say
    // which preset is on
    const bool keep_cycle_position = Controller_has_cycle(self) && !Controller_may_show_in_combobox(self, current);

    // The raw value, so a value out of range is put right too
    if (!keep_cycle_position
        && *self->current_preset_index != (float) current
        && control_input_port->request_change(control_input_port->handle, PORT_PRESET_SELECT, (float) current) == LV2_CONTROL_INPUT_PORT_CHANGE_SUCCESS) {
        self->combobox = current;
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

    if (!strcmp(uri, LV2_WORKER__interface)) {
        return &worker;
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
        lv2_log_error(&self->lv2->logger, PLUGIN_NAME ": malformed patch:Set property\n");
        return;
    }

    if (value == NULL || value->type != self->lv2->uris.atom_String) {
        lv2_log_error(&self->lv2->logger, PLUGIN_NAME ": invalid preset label value\n");
        return;
    }

    LV2_URID property_urid = ((const LV2_Atom_URID*) property)->body;
    const char* label = (const char*) (value + 1);

    for (unsigned int i=0; i<TOTAL_PRESETS; i++) {
        if (self->lv2->uris.preset_label[i] != property_urid) {
            continue;
        }

        if (!Controller_set_preset_label(self, i, label)) {
            lv2_log_error(&self->lv2->logger, PLUGIN_NAME ": could not store preset label\n");
        }

        return;
    }

    lv2_log_error(&self->lv2->logger, PLUGIN_NAME ": unknown patch property\n");
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
        lv2_log_error(&self->lv2->logger, PLUGIN_NAME ": malformed patch:Get property\n");
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

    lv2_log_error(&self->lv2->logger, PLUGIN_NAME ": unknown patch:Get property\n");
}

static void Controller_send_restored_labels(Controller* self) {
    if (!self->lv2->labels_restored || self->lv2->events_out == NULL) {
        return;
    }

    self->lv2->labels_restored = false;

    for (unsigned int i=0; i<TOTAL_PRESETS; i++) {
        Controller_send_patch_set(self, 0, self->lv2->uris.preset_label[i], Controller_get_preset_label(self, i));
    }
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
            lv2_log_error(&self->lv2->logger, PLUGIN_NAME ": invalid stored preset label type\n");
            continue;
        }

        if (!Controller_set_preset_label(self, i, (const char*) value)) {
            lv2_log_error(&self->lv2->logger, PLUGIN_NAME ": could not restore preset label\n");
            continue;
        }

        self->lv2->labels_restored = true;
    }

    return LV2_STATE_SUCCESS;
}

static const LV2_Descriptor descriptor = {
    SWITCH_PLUGIN_URI,
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
