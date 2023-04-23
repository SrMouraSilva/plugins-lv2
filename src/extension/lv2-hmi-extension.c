#include <stdlib.h>
#include <stdbool.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#include "lv2/core/lv2_util.h"
#include <lv2/lv2plug.in/ns/ext/log/log.h>

#include "../extension/lv2-hmi.h"
#include "./control-input-port-change-request.h"
#include "../model/tetr4-switch.h"

typedef enum {
    PRESET_SELECTOR_1 = 4,
    PRESET_SELECTOR_2 = 5,
    PRESET_SELECTOR_3 = 6,
    PRESET_SELECTOR_4 = 7,

    PRESET_COMBOBOX = 8,
    ASSIGN_TO_NOTIFY = 9,
} HmiAdressing;

LV2_HMI_LED_Colour led_colours[] = {
    LV2_HMI_LED_Colour_Red, LV2_HMI_LED_Colour_Yellow, LV2_HMI_LED_Colour_Cyan, LV2_HMI_LED_Colour_Magenta,
    LV2_HMI_LED_Colour_Magenta, LV2_HMI_LED_Colour_Yellow, LV2_HMI_LED_Colour_White
};


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
        LV2_LOG__log,                              &self->logger.log,         false,
        LV2_URID__map,                             &self->map,                true,
        LV2_HMI__WidgetControl,                    &self->hmi,                true,
        LV2_CONTROL_INPUT_PORT_CHANGE_REQUEST_URI, &self->control_input_port, true,
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

static void LV2_HMI_addressed(LV2_Handle handle, uint32_t index, LV2_HMI_Addressing addressing, const LV2_HMI_AddressingInfo* info) {
    Tetr4Switch* self = (Tetr4Switch*) handle;
    
    LV2_HMI_assign(self, (HmiAdressing) index, addressing);

    // self->infos[index] = *info;
    // self->infos[index].label = strdup(info->label);
}

static void LV2_HMI_unaddressed(LV2_Handle handle, uint32_t index) {
    Tetr4Switch* self = (Tetr4Switch*) handle;

    LV2_HMI_assign(self, (HmiAdressing) index, NULL);

    // free((void*)self->infos[index].label);
    // memset(&self->infos[index], 0, sizeof(self->infos[index]));
}

void LV2_HMI_assign(Tetr4Switch* self, HmiAdressing index, LV2_HMI_Addressing addressing) {
    switch (index) {
        case PRESET_SELECTOR_1:
        case PRESET_SELECTOR_2:
        case PRESET_SELECTOR_3:
        case PRESET_SELECTOR_4:
            self->hmi_preset_addressing[index-PRESET_SELECTOR_1] = addressing; break;
        case ASSIGN_TO_NOTIFY:
            self->hmi_notification_addressing = addressing; break;
        case PRESET_COMBOBOX:
            self->hmi_combobox_addressing = addressing; break;
    }
}


void run_footswitches(Tetr4Switch* self, unsigned int current_preset);
void run_combobox(Tetr4Switch* self, unsigned int current_preset);
void run_notification(Tetr4Switch* self, unsigned int current_preset);

void LV2_HMI_run(Tetr4Switch* self) {
    unsigned int current_preset = self->get_index_current_preset(self);

    run_footswitches(self, current_preset);
    run_combobox(self, current_preset);
    run_notification(self, current_preset);
}

void run_footswitches(Tetr4Switch* self, unsigned int current_preset) {
    for (unsigned int i = 0; i<TOTAL_PRESETS; i++) {
        if (self->hmi_preset_addressing[i] != NULL) {
            LV2_HMI_LED_Brightness brightness = i == current_preset
                ? LV2_HMI_LED_Brightness_Normal
                : LV2_HMI_LED_Brightness_None;
            
            char message[11];
            sprintf(message, "%d - %s", i+1, i == current_preset ? "ON" : "OFF");

            self->hmi->set_led_with_brightness(
                self->hmi->handle,
                self->hmi_preset_addressing[i],
                led_colours[i],
                brightness
            );
            self->hmi->set_label(self->hmi->handle, self->hmi_preset_addressing[i], message);
        }
    }
}

void run_combobox(Tetr4Switch* self, unsigned int current_preset) {
    if (self->hmi_combobox_addressing != NULL) {
        LV2_HMI_LED_Colour current_led_colour = led_colours[current_preset];

        self->hmi->set_led_with_brightness(self->hmi->handle, self->hmi_combobox_addressing, current_led_colour, LV2_HMI_LED_Brightness_Normal);


        char short_message[10];
        //sprintf(short_message, "%d PRESET", current_preset+1);
        sprintf(short_message, "%d", current_preset+1);

        //self->hmi->set_label(self->hmi->handle, self->hmi_combobox_addressing, "");
        self->hmi->set_label(self->hmi->handle, self->hmi_combobox_addressing, short_message);
        //self->hmi->set_indicator(self->hmi->handle, self->hmi_combobox_addressing, current_preset+1/TOTAL_PRESETS);

        if (self->preset_changed) {
            lv2_log_error(&self->logger, "Changed to <%s>\n", short_message);
        }
    }
}

void run_notification(Tetr4Switch* self, unsigned int current_preset) {
    if (self->preset_changed && self->hmi_notification_addressing != NULL) {
        char message[10];
        sprintf(message, "Preset %d", current_preset+1);

        self->hmi->popup_message(
            self->hmi->handle,
            self->hmi_notification_addressing,
            LV2_HMI_Popup_Style_Inverted,
            "Tetr4 Switch",
            message
        );
    }
}