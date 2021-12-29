#include <stdlib.h>
#include <string.h>

#include "lv2/atom/atom.h"

#include "../model/tetr4-switch.h"
#include "../utils/utils.h"


char* get_new_preset_label(Tetr4Switch* self, LV2_Atom* atom, unsigned int current_value);

void LV2_HMI_instantiate(Tetr4Switch* self) {
    URIs* const uris = &self->uris;

    for (int i=0; i<TOTAL_PRESETS; i++) {
        char* index_as_string = integer_to_string(i);

        uris->atom_Presets_label[i] = self->map->map(
            self->map->handle,
            strcat(PLUGIN_URI "#preset_label_", index_as_string)
        );

        free(index_as_string);
    }
}


void Atom_run(Tetr4Switch* self) {
    const URIs* uris = &self->uris;

    for (int i=0; i<TOTAL_PRESETS; i++) {
        LV2_Atom* preset_label = NULL;
        
        lv2_atom_object_get(
            obj, // FIXME
            uris->atom_Presets_label[i], &preset_label,
            NULL
        );

        char* new_preset_label = get_new_preset_label(self, preset_label, i);
        self->set_preset_label(self, i, new_preset_label);
    }

    // LV2_ATOM_SEQUENCE_FOREACH(self->events_in, ev) {
    // }
}


char* get_new_preset_label(Tetr4Switch* self, LV2_Atom* atom, unsigned int index_preset) {
    const URIs* uris = &self->uris;

    if (atom != NULL && atom->type == uris->atom_String) {
        LV2_Atom_String* string = (LV2_Atom_String*) atom;
        char* new_value_string = string->body;
        return new_value_string;
    }

    return self->get_preset_label(self, index_preset);
}
