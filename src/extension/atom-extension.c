#include <stdlib.h>
#include <string.h>

#include "lv2/atom/atom.h"
#include "lv2/atom/util.h"

#include "../model/tetr4-switch.h"
#include "../utils/utils.h"


#define PRESET_LABEL_1_URI PLUGIN_URI "#preset_label_1"
#define PRESET_LABEL_2_URI PLUGIN_URI "#preset_label_2"
#define PRESET_LABEL_3_URI PLUGIN_URI "#preset_label_3"
#define PRESET_LABEL_4_URI PLUGIN_URI "#preset_label_4"


char* get_new_preset_label(Tetr4Switch* self, LV2_Atom* atom, unsigned int current_value);


void Atom_instantiate(Tetr4Switch* self) {
    // FIXME
    URIs* const uris = &self->uris;

    uris->atom_presets_label[0] = self->map->map(self->map->handle, PRESET_LABEL_1_URI);
    uris->atom_presets_label[1] = self->map->map(self->map->handle, PRESET_LABEL_2_URI);
    uris->atom_presets_label[2] = self->map->map(self->map->handle, PRESET_LABEL_3_URI);
    uris->atom_presets_label[3] = self->map->map(self->map->handle, PRESET_LABEL_4_URI);

    // for (int i=0; i<TOTAL_PRESETS; i++) {
    //     char* index_as_string = integer_to_string(i);

    //     uris->atom_presets_label[i] = self->map->map(
    //         self->map->handle,
    //         strcat(PLUGIN_URI "#preset_label_", index_as_string)
    //     );

    //     free(index_as_string);
    // }
}

void Atom_run(Tetr4Switch* self) {
    LV2_ATOM_SEQUENCE_FOREACH(self->events_in, ev) {
        const LV2_Atom_Object* obj = (const LV2_Atom_Object*) &ev->body;


        // lv2_atom_object_get(
        //     obj, // FIXME
        //     uris->atom_Presets_label[i], &preset_label,
        //     NULL
        // );
    }

    // FIXME
    // const URIs* uris = &self->uris;

    // for (int i=0; i<TOTAL_PRESETS; i++) {
    //     LV2_Atom* preset_label = NULL;
        
    //     lv2_atom_object_get(
    //         obj, // FIXME
    //         uris->atom_Presets_label[i], &preset_label,
    //         NULL
    //     );

    //     char* new_preset_label = get_new_preset_label(self, preset_label, i);
    //     self->set_preset_label(self, i, new_preset_label);
    // }
}


char* get_new_preset_label(Tetr4Switch* self, LV2_Atom* atom, unsigned int index_preset) {
    const URIs* uris = &self->uris;

    // FIXME
    // if (atom != NULL && atom->type == uris->atom_String) {
    //     LV2_Atom_String* string = (LV2_Atom_String*) atom;
    //     char* new_value_string = string->body;
    //     return new_value_string;
    // }

    return self->get_preset_label(self, index_preset);
}
