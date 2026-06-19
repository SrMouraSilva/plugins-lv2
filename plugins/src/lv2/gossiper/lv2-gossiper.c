#include <stdlib.h>

#include <lv2/core/lv2_util.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/patch/patch.h>

#include "lv2-gossiper.h"


const char* instantiate(
    void* self,
    const LV2_Feature* const* features
);

static void map_uris(LV2_URID_Map* map, Gossiper_URIs* uris);

LV2_Gossiper* LV2_Gossiper_instantiate() {
    LV2_Gossiper* self = (LV2_Gossiper*) calloc(1, sizeof(LV2_Gossiper));

    if (self == NULL) {
        return NULL;
    }

    self->initialize = &instantiate;

    return self;
}


const char* instantiate(
    void* self,
    const LV2_Feature* const* features
) {
    LV2_Gossiper* this = (LV2_Gossiper*) self;

    // Get host features
    const char* missing = lv2_features_query(
        features,
        LV2_LOG__log,                              &this->logger.log,         false,
        LV2_URID__map,                             &this->map,                true,
        LV2_HMI__WidgetControl,                    &this->hmi,                true,
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

static void map_uris(LV2_URID_Map* map, Gossiper_URIs* uris) {
    uris->atom_Object = map->map(map->handle, LV2_ATOM__Object);
    uris->atom_URID = map->map(map->handle, LV2_ATOM__URID);
    uris->atom_String = map->map(map->handle, LV2_ATOM__String);
    uris->patch_Set = map->map(map->handle, LV2_PATCH__Set);
    uris->patch_property = map->map(map->handle, LV2_PATCH__property);
    uris->patch_value = map->map(map->handle, LV2_PATCH__value);

    uris->footswitch_label[0] = map->map(map->handle, GOSSIPER_PLUGIN_URI "#footswitch_label_1");
    uris->footswitch_label[1] = map->map(map->handle, GOSSIPER_PLUGIN_URI "#footswitch_label_2");
    uris->footswitch_label[2] = map->map(map->handle, GOSSIPER_PLUGIN_URI "#footswitch_label_3");
    uris->footswitch_label[3] = map->map(map->handle, GOSSIPER_PLUGIN_URI "#footswitch_label_4");
    uris->footswitch_label[4] = map->map(map->handle, GOSSIPER_PLUGIN_URI "#footswitch_label_5");
    uris->footswitch_label[5] = map->map(map->handle, GOSSIPER_PLUGIN_URI "#footswitch_label_6");
    uris->footswitch_label[6] = map->map(map->handle, GOSSIPER_PLUGIN_URI "#footswitch_label_7");
}