#include <stdlib.h>

#include <lv2/core/lv2_util.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/patch/patch.h>

#include "lv2-controller.h"



const char* LV2_Controller_initialize(
    void* self,
    const LV2_Feature* const* features
);

static void map_uris(LV2_URID_Map* map, Controller_URIs* uris);

LV2_Controller* LV2_Controller_instantiate() {
    LV2_Controller* self = (LV2_Controller*) calloc(1, sizeof(LV2_Controller));

    if (self == NULL) {
        return NULL;
    }

    self->initialize = &LV2_Controller_initialize;

    for (unsigned int i = 0; i<TOTAL_PRESETS; i++) {
        self->hmi.preset[i] = NULL;
    }
    self->hmi.select = NULL;
    
    for (unsigned int i=0; i<TOTAL_CONTROLLER_NOTIFIERS; i++) {
        self->hmi.notifiers[i] = NULL;
    }

    return self;
}


const char* LV2_Controller_initialize(
    void* self,
    const LV2_Feature* const* features
) {
    LV2_Controller* this = (LV2_Controller*) self;

    // Get host features
    const char* missing = lv2_features_query(
        features,
        LV2_LOG__log,                              &this->logger.log,         false,
        LV2_URID__map,                             &this->map,                true,
        LV2_HMI__WidgetControl,                    &this->hmi,                true,
        LV2_CONTROL_INPUT_PORT_CHANGE_REQUEST_URI, &this->control_input_port, true,
        NULL
    );

    lv2_log_logger_set_map(&this->logger, this->map);

    if (missing) {
        lv2_log_error(&this->logger, "Missing feature <%s>\n", missing);
        return missing;
    }

    map_uris(this->map, &this->uris);

    return NULL;
}

static void map_uris(LV2_URID_Map* map, Controller_URIs* uris) {
    uris->atom_Object = map->map(map->handle, LV2_ATOM__Object);
    uris->atom_URID = map->map(map->handle, LV2_ATOM__URID);
    uris->atom_String = map->map(map->handle, LV2_ATOM__String);
    uris->patch_Set = map->map(map->handle, LV2_PATCH__Set);
    uris->patch_property = map->map(map->handle, LV2_PATCH__property);
    uris->patch_value = map->map(map->handle, LV2_PATCH__value);

    uris->preset_label[0] = map->map(map->handle, TETR4_SWITCH_PLUGIN_URI "#preset_label_1");
    uris->preset_label[1] = map->map(map->handle, TETR4_SWITCH_PLUGIN_URI "#preset_label_2");
    uris->preset_label[2] = map->map(map->handle, TETR4_SWITCH_PLUGIN_URI "#preset_label_3");
    uris->preset_label[3] = map->map(map->handle, TETR4_SWITCH_PLUGIN_URI "#preset_label_4");
}