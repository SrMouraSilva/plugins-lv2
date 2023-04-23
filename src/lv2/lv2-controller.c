#include <stdlib.h>

#include <lv2/core/lv2_util.h>

#include "lv2-controller.h"



const char* LV2_Controller_initialize(
    void* self,
    const LV2_Feature* const* features
);

LV2_Controller* LV2_Controller_instantiate() {
    LV2_Controller* self = (LV2_Controller*) malloc(sizeof(LV2_Controller));

    self->initialize = &LV2_Controller_initialize;

    for (unsigned int i = 0; i<TOTAL_PRESETS; i++) {
        self->hmi.preset[i] = NULL;
    }
    self->hmi.select = NULL;
    self->hmi.notification = NULL;

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
    }

    return missing;
}