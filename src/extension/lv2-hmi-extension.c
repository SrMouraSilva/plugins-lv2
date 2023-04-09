#include <stdlib.h>
#include <stdbool.h>

#include "lv2/log/logger.h"

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#include "lv2/core/lv2_util.h"
#include "lv2/log/log.h"

#include "../lv2-hmi.h"
#include "../model/tetr4-switch.h"

typedef enum {
    LED = 0,
    LABEL = 1,
} HmiAdressing;


static void LV2_HMI_addressed(LV2_Handle handle, uint32_t index, LV2_HMI_Addressing addressing, const LV2_HMI_AddressingInfo* info);
static void LV2_HMI_unaddressed(LV2_Handle handle, uint32_t index);
static void LV2_HMI_assign(Tetr4Switch* self, HmiAdressing index, LV2_HMI_Addressing addressing);


const char* LV2_HMI_instantiate(
    Tetr4Switch* self,
    const LV2_Feature* const* features
) {
    // Get host features
    const char* missing = lv2_features_query(
        features,
        LV2_LOG__log,           &self->logger.log, false,
        LV2_URID__map,          &self->map,        true,
        LV2_HMI__WidgetControl, &self->hmi,        true,
        NULL
    );

    lv2_log_logger_set_map(&self->logger, self->map);

    if (missing) {
        lv2_log_error(&self->logger, "Missing feature <%s>\n", missing);
    }

    return missing;
}

bool LV2_HMI_is_extension_data_appliable(const char* uri) {
  return (!strcmp(uri, LV2_HMI__PluginNotification));
}

const void* LV2_HMI_extension_data() {
    static const LV2_HMI_PluginNotification hmiNotif = {
        LV2_HMI_addressed,
        LV2_HMI_unaddressed,
    };

    return &hmiNotif;
}

void LV2_HMI_addressed(LV2_Handle handle, uint32_t index, LV2_HMI_Addressing addressing, const LV2_HMI_AddressingInfo* info) {
    Tetr4Switch* self = (Tetr4Switch*) handle;

    LV2_HMI_assign(self, (HmiAdressing) index, addressing);

    // self->infos[index] = *info;
    // self->infos[index].label = strdup(info->label);
}

void LV2_HMI_unaddressed(LV2_Handle handle, uint32_t index) {
    Tetr4Switch* self = (Tetr4Switch*) handle;

    LV2_HMI_assign(self, (HmiAdressing) index, NULL);

    // free((void*)self->infos[index].label);
    // memset(&self->infos[index], 0, sizeof(self->infos[index]));
}

void LV2_HMI_assign(Tetr4Switch* self, HmiAdressing index, LV2_HMI_Addressing addressing) {
    switch (index) {
        case LED:
            self->hmi_led_addressing = addressing; break;
        case LABEL:
            self->hmi_label_addressing = addressing; break;
    }
}



void LV2_HMI_run(Tetr4Switch* self) {
    unsigned int current_preset = self->get_index_current_preset(self);

    LV2_HMI_LED_Colour led_colous[] = { LV2_HMI_LED_Colour_Red, LV2_HMI_LED_Colour_Yellow, LV2_HMI_LED_Colour_Cyan, LV2_HMI_LED_Colour_Magenta };
    LV2_HMI_LED_Colour current_led_colour = led_colous[current_preset];

    self->hmi->set_led_with_brightness(self->hmi->handle, self->hmi_led_addressing, current_led_colour, LV2_HMI_LED_Brightness_Normal);

    self->hmi->set_label(self->hmi->handle, self->hmi_label_addressing, self->preset_labels[current_preset]);
}