#include <stdlib.h>

#include <lv2/core/lv2_util.h>

#include "lv2-gossiper.h"


const char* instantiate(
    void* self,
    const LV2_Feature* const* features
);

LV2_Gossiper* LV2_Gossiper_instantiate() {
    LV2_Gossiper* self = (LV2_Gossiper*) malloc(sizeof(LV2_Gossiper));

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
    }

    return missing;
}