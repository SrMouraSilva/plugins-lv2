#include <stdlib.h>
#include <stdbool.h>

#include <lv2/core/lv2.h>
#include <lv2/core/lv2_util.h>
#include <lv2/lv2plug.in/ns/ext/log/log.h>

#include "../extension/lv2-hmi.h"
#include "../extension/control-input-port-change-request.h"

#include "../../model/controller.h"

typedef enum {
    PRESET_SELECTOR_1 = 4,
    PRESET_SELECTOR_2 = 5,
    PRESET_SELECTOR_3 = 6,
    PRESET_SELECTOR_4 = 7,

    PRESET_SELECT = 8,
    ASSIGN_TO_NOTIFY = 9,
} HmiAdressing;

LV2_HMI_LED_Colour led_colours[] = {
    LV2_HMI_LED_Colour_Red, LV2_HMI_LED_Colour_Yellow, LV2_HMI_LED_Colour_Cyan, LV2_HMI_LED_Colour_Magenta,
    LV2_HMI_LED_Colour_Magenta, LV2_HMI_LED_Colour_Yellow, LV2_HMI_LED_Colour_White
};


static void LV2_HMI_addressed(LV2_Handle handle, uint32_t index, LV2_HMI_Addressing addressing, const LV2_HMI_AddressingInfo* info);
static void LV2_HMI_unaddressed(LV2_Handle handle, uint32_t index);
static void LV2_HMI_assign(Controller* self, HmiAdressing index, LV2_HMI_Addressing addressing);


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
    Controller* self = (Controller*) handle;
    
    LV2_HMI_assign(self, (HmiAdressing) index, addressing);

    // self->infos[index] = *info;
    // self->infos[index].label = strdup(info->label);
}

static void LV2_HMI_unaddressed(LV2_Handle handle, uint32_t index) {
    Controller* self = (Controller*) handle;

    LV2_HMI_assign(self, (HmiAdressing) index, NULL);

    // free((void*)self->infos[index].label);
    // memset(&self->infos[index], 0, sizeof(self->infos[index]));
}

void run_footswitches(Controller* self, unsigned int current_preset);
void run_footswitch(Controller* self, unsigned int index_footswitch, bool on, bool force_update);
void run_select(Controller* self, unsigned int current_preset, bool force_update);
void run_notification(Controller* self, unsigned int current_preset, bool force_update);

void LV2_HMI_assign(Controller* self, HmiAdressing index, LV2_HMI_Addressing addressing) {
    unsigned int current_preset = self->get_index_current_preset(self);

    unsigned int i = index - PRESET_SELECTOR_1;

    switch (index) {
        case PRESET_SELECTOR_1:
        case PRESET_SELECTOR_2:
        case PRESET_SELECTOR_3:
        case PRESET_SELECTOR_4:
            self->lv2->hmi.preset[i] = addressing;
            run_footswitch(self, i, i == current_preset, true);
            break;
        case PRESET_SELECT:
            self->lv2->hmi.select = addressing;
            run_select(self, current_preset, true);
            break;
        case ASSIGN_TO_NOTIFY:
            self->lv2->hmi.notification = addressing;
            run_notification(self, current_preset, true);
            break;
    }
}


void LV2_HMI_run(Controller* self) {
    unsigned int current_preset = self->get_index_current_preset(self);

    if (self->is_preset_changed(self)) {
        lv2_log_error(&self->lv2->logger, "Changed to <PRESET %d>\n", current_preset);
    }

    run_footswitches(self, current_preset);
    run_select(self, current_preset, false);
    run_notification(self, current_preset, false);
}

void run_footswitches(Controller* self, unsigned int current_preset) {
    for (unsigned int i = 0; i<TOTAL_PRESETS; i++) {
        run_footswitch(self, i, i == current_preset, false);
    }
}

void run_footswitch(Controller* self, unsigned int index_footswitch, bool on, bool force_update) {
    HMI* hmi = &self->lv2->hmi;

    LV2_HMI_Addressing address = hmi->preset[index_footswitch];

    if (address != NULL && (force_update || self->is_preset_changed(self))) {
        LV2_HMI_LED_Brightness brightness = on
            ? LV2_HMI_LED_Brightness_Normal
            : LV2_HMI_LED_Brightness_None;
        
        char message[11];
        sprintf(message, "%d - %s", index_footswitch+1, on ? "ON" : "OFF");

        hmi->widgetControl->set_led_with_brightness(
            hmi->widgetControl->handle,
            address,
            led_colours[index_footswitch],
            brightness
        );

        hmi->widgetControl->set_label(
            hmi->widgetControl->handle,
            address,
            message
        );
    }
}

void run_select(Controller* self, unsigned int current_preset, bool force_update) {
    HMI* hmi = &self->lv2->hmi;

    if (hmi->select != NULL && (force_update || self->is_preset_changed(self))) {
        LV2_HMI_LED_Colour current_led_colour = led_colours[current_preset];

        hmi->widgetControl->set_led_with_brightness(
            hmi->widgetControl->handle,
            hmi->select,
            current_led_colour,
            LV2_HMI_LED_Brightness_Normal
        );

        char short_message[10];
        //sprintf(short_message, "%d PRESET", current_preset+1);
        sprintf(short_message, "%d", current_preset+1);

        //hmi->widgetControl->set_label(hmi->widgetControl->handle, hmi->select, "");
        hmi->widgetControl->set_label(hmi->widgetControl->handle, hmi->select, short_message);
        //hmi->widgetControl->set_indicator(hmi->widgetControl->handle, hmi->select, current_preset+1/TOTAL_PRESETS);
    }
}

void run_notification(Controller* self, unsigned int current_preset, bool force_update) {
    HMI* hmi = &self->lv2->hmi;

    if (hmi->notification != NULL && (force_update || self->is_preset_changed(self))) {
        char message[10];
        sprintf(message, "Preset %d", current_preset+1);

        hmi->widgetControl->popup_message(
            hmi->widgetControl->handle,
            hmi->notification,
            LV2_HMI_Popup_Style_Inverted,
            PLUGIN_NAME,
            message
        );
    }
}
