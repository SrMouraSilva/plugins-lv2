/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <stdbool.h>

#include <lv2/core/lv2.h>
#include <lv2/core/lv2_util.h>
#include <lv2/lv2plug.in/ns/ext/log/log.h>

#include "../extension/lv2-hmi.h"
#include "../extension/control-input-port-change-request.h"

#include "../../model/controller.h"
#include "../../model/ports.h"

/** Colour of each preset's LED. The HMI has seven of them, which is why Hept7 Switch has seven presets */
static const LV2_HMI_LED_Colour led_colours[TOTAL_PRESETS] = {
#ifdef HEPT7_SWITCH
    LV2_HMI_LED_Colour_Red, LV2_HMI_LED_Colour_Yellow, LV2_HMI_LED_Colour_Green, LV2_HMI_LED_Colour_Cyan,
    LV2_HMI_LED_Colour_Blue, LV2_HMI_LED_Colour_Magenta, LV2_HMI_LED_Colour_White,
#else
    LV2_HMI_LED_Colour_Red, LV2_HMI_LED_Colour_Yellow, LV2_HMI_LED_Colour_Cyan, LV2_HMI_LED_Colour_Magenta,
#endif
};


static void LV2_HMI_addressed(LV2_Handle handle, uint32_t index, LV2_HMI_Addressing addressing, const LV2_HMI_AddressingInfo* info);
static void LV2_HMI_unaddressed(LV2_Handle handle, uint32_t index);
static void LV2_HMI_assign(Controller* self, uint32_t index, LV2_HMI_Addressing addressing);


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
    
    LV2_HMI_assign(self, index, addressing);

    // self->infos[index] = *info;
    // self->infos[index].label = strdup(info->label);
}

static void LV2_HMI_unaddressed(LV2_Handle handle, uint32_t index) {
    Controller* self = (Controller*) handle;

    LV2_HMI_assign(self, index, NULL);

    // free((void*)self->infos[index].label);
    // memset(&self->infos[index], 0, sizeof(self->infos[index]));
}

void run_footswitches(Controller* self, unsigned int current_preset);
void run_footswitch(Controller* self, unsigned int index_footswitch, bool on, bool force_update);
void run_select(Controller* self, unsigned int current_preset, bool force_update);
void run_notification(Controller* self, unsigned int current_preset, bool force_update);

void LV2_HMI_assign(Controller* self, uint32_t index, LV2_HMI_Addressing addressing) {
    unsigned int current_preset = self->get_index_current_preset(self);

    if (index >= PORT_PRESET_SELECTOR_FIRST && index < PORT_PRESET_SELECT) {
        unsigned int preset_index = index - PORT_PRESET_SELECTOR_FIRST;

        self->lv2->hmi.preset[preset_index] = addressing;
        run_footswitch(self, preset_index, preset_index == current_preset, true);

    } else if (index == PORT_PRESET_SELECT) {
        self->lv2->hmi.select = addressing;
        run_select(self, current_preset, true);

    } else if (index >= PORT_ASSIGN_TO_NOTIFY_FIRST && index < PORT_PRESET_OUTPUT_FIRST) {
        unsigned int notifier_index = index - PORT_ASSIGN_TO_NOTIFY_FIRST;

        self->lv2->hmi.notifiers[notifier_index] = addressing;
        run_notification(self, current_preset, true);
    }
}


void LV2_HMI_run(Controller* self) {
    unsigned int current_preset = self->get_index_current_preset(self);

    //if (self->is_preset_changed(self)) {
    //    lv2_log_error(&self->lv2->logger, "Changed to <PRESET %d>\n", current_preset);
    //}

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

    if (hmi->widgetControl == NULL) {
        return;
    }

    LV2_HMI_Addressing address = hmi->preset[index_footswitch];

    if (address != NULL && (force_update || self->is_preset_changed(self))) {
        LV2_HMI_LED_Brightness brightness = on
            ? LV2_HMI_LED_Brightness_Normal
            : LV2_HMI_LED_Brightness_None;
        
        char message[11];
        snprintf(message, sizeof(message), "%d - %s", index_footswitch+1, on ? "ON" : "OFF");

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

    if (hmi->widgetControl == NULL) {
        return;
    }

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
        snprintf(short_message, sizeof(short_message), "%d", current_preset+1);

        //hmi->widgetControl->set_label(hmi->widgetControl->handle, hmi->select, "");
        hmi->widgetControl->set_label(hmi->widgetControl->handle, hmi->select, short_message);
        //hmi->widgetControl->set_indicator(hmi->widgetControl->handle, hmi->select, current_preset+1/TOTAL_PRESETS);
    }
}

void run_notification(Controller* self, unsigned int current_preset, bool force_update) {
    HMI* hmi = &self->lv2->hmi;

    if (hmi->widgetControl == NULL) {
        return;
    }

    if (force_update || self->is_preset_changed(self)) {
        if (!Controller_is_popup_enabled(self, current_preset)) {
            return;
        }

        const char* message = Controller_get_preset_label(self, current_preset);

        if (message == NULL) {
            return;
        }

        for (unsigned int i=0; i<TOTAL_CONTROLLER_NOTIFIERS; i++) {
            hmi->widgetControl->popup_message(
                hmi->widgetControl->handle,
                hmi->notifiers[i],
                LV2_HMI_Popup_Style_Inverted,
                PLUGIN_NAME,
                message
            );
        }
    }
}
