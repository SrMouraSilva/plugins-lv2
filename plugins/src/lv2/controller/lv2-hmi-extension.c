/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <lv2/core/lv2.h>
#include <lv2/core/lv2_util.h>
#include <lv2/lv2plug.in/ns/ext/log/log.h>

#include "../extension/lv2-hmi.h"
#include "../extension/control-input-port-change-request.h"

#include "../../model/controller.h"
#include "../../model/ports.h"

#include "lv2-hmi-extension.h"

/** Colour of each preset's LED. The HMI has seven of them, which is why Hept7 Switch has seven presets */
static const LV2_HMI_LED_Colour led_colours[TOTAL_PRESETS] = {
#ifdef HEPT7_SWITCH
    LV2_HMI_LED_Colour_Red, LV2_HMI_LED_Colour_Yellow, LV2_HMI_LED_Colour_Green, LV2_HMI_LED_Colour_Cyan,
    LV2_HMI_LED_Colour_Blue, LV2_HMI_LED_Colour_Magenta, LV2_HMI_LED_Colour_White,
#else
    LV2_HMI_LED_Colour_Red, LV2_HMI_LED_Colour_Yellow, LV2_HMI_LED_Colour_Cyan, LV2_HMI_LED_Colour_Magenta,
#endif
};

/**
 * A change of preset, as run() hands it to the worker. The label is copied: run() may change it (a patch:Set) while
 * the worker is still sending it
 */
typedef struct {
    uint32_t current_preset;
    uint32_t previous_preset;
    /** "Current preset" goes through a cycle: the device's own value for it may not be the active preset */
    uint32_t show_value;
    char label[PRESET_LABEL_MAX_SIZE];
} HMI_PresetChange;


static void LV2_HMI_addressed(LV2_Handle handle, uint32_t index, LV2_HMI_Addressing addressing, const LV2_HMI_AddressingInfo* info);
static void LV2_HMI_unaddressed(LV2_Handle handle, uint32_t index);
static void LV2_HMI_assign(Controller* self, uint32_t index, LV2_HMI_Addressing addressing);
static void LV2_HMI_send_preset_change(Controller* self, const HMI_PresetChange* change);

static void show_footswitch(HMI* hmi, unsigned int preset, bool on);
static void show_select(HMI* hmi, unsigned int current_preset, const char* value);
static void show_popups(HMI* hmi, const char* label);


bool LV2_HMI_is_extension_data_appliable(const char* uri) {
  return (!strcmp(uri, LV2_HMI__PluginNotification));
}

const void* LV2_HMI_extension_data(void) {
    static const LV2_HMI_PluginNotification hmiNotif = {
        LV2_HMI_addressed,
        LV2_HMI_unaddressed,
    };

    return &hmiNotif;
}

// Called by the host from a non real-time thread, at the same time as run() may be running
static void LV2_HMI_addressed(LV2_Handle handle, uint32_t index, LV2_HMI_Addressing addressing, const LV2_HMI_AddressingInfo* info) {
    LV2_HMI_assign((Controller*) handle, index, addressing);
}

static void LV2_HMI_unaddressed(LV2_Handle handle, uint32_t index) {
    LV2_HMI_assign((Controller*) handle, index, NULL);
}

/**
 * Keep the addressing, and show the state on the widget that has just been addressed (and only on it).
 * A notifier shows nothing here: a pop-up is only for a change of preset (otherwise loading a pedalboard with the
 * notifiers addressed would show one pop-up per notifier, one after the other)
 */
static void LV2_HMI_assign(Controller* self, uint32_t index, LV2_HMI_Addressing addressing) {
    HMI* hmi = &self->lv2->hmi;
    const unsigned int current_preset = self->current_preset;

    if (index >= PORT_PRESET_SELECTOR_FIRST && index < PORT_PRESET_SELECT) {
        const unsigned int preset = index - PORT_PRESET_SELECTOR_FIRST;

        hmi->preset[preset] = addressing;
        show_footswitch(hmi, preset, preset == current_preset);

    } else if (index == PORT_PRESET_SELECT) {
        hmi->select = addressing;
        show_select(hmi, current_preset, NULL);

    } else if (index >= PORT_ASSIGN_TO_NOTIFY_FIRST && index < PORT_PRESET_OUTPUT_FIRST) {
        hmi->notifiers[index - PORT_ASSIGN_TO_NOTIFY_FIRST] = addressing;
    }
}


void LV2_HMI_run(Controller* self) {
    if (!self->preset_changed || self->lv2->hmi.widgetControl == NULL) {
        return;
    }

    HMI_PresetChange change;
    change.current_preset = self->current_preset;
    change.previous_preset = self->previous_preset;
    change.show_value = Controller_has_cycle(self);
    memcpy(change.label, self->preset_labels[self->current_preset], PRESET_LABEL_MAX_SIZE);

    LV2_Worker_Schedule* worker = self->lv2->worker;

    if (worker != NULL && worker->schedule_work(worker->handle, sizeof(change), &change) == LV2_WORKER_SUCCESS) {
        return;
    }

    // No worker (or its queue is full): send from here, as before
    LV2_HMI_send_preset_change(self, &change);
}

LV2_Worker_Status LV2_HMI_work(LV2_Handle instance,
                               LV2_Worker_Respond_Function respond,
                               LV2_Worker_Respond_Handle handle,
                               uint32_t size,
                               const void* data) {
    if (size != sizeof(HMI_PresetChange)) {
        return LV2_WORKER_ERR_UNKNOWN;
    }

    LV2_HMI_send_preset_change((Controller*) instance, (const HMI_PresetChange*) data);

    return LV2_WORKER_SUCCESS;
}

LV2_Worker_Status LV2_HMI_work_response(LV2_Handle instance, uint32_t size, const void* body) {
    // The worker never responds
    return LV2_WORKER_SUCCESS;
}

/** Only what the change touches: the footswitch turned off, the one turned on, the select, the pop-ups */
static void LV2_HMI_send_preset_change(Controller* self, const HMI_PresetChange* change) {
    HMI* hmi = &self->lv2->hmi;

    if (hmi->widgetControl == NULL
        || change->current_preset >= TOTAL_PRESETS
        || change->previous_preset >= TOTAL_PRESETS) {
        return;
    }

    if (change->previous_preset != change->current_preset) {
        show_footswitch(hmi, change->previous_preset, false);
    }
    show_footswitch(hmi, change->current_preset, true);
    show_select(hmi, change->current_preset, change->show_value ? change->label : NULL);
    show_popups(hmi, change->label);
}


static void show_footswitch(HMI* hmi, unsigned int preset, bool on) {
    const LV2_HMI_Addressing address = hmi->preset[preset];

    if (hmi->widgetControl == NULL || address == NULL) {
        return;
    }

    char message[16];
    snprintf(message, sizeof(message), "%u - %s", preset + 1, on ? "ON" : "OFF");

    hmi->widgetControl->set_led_with_brightness(
        hmi->widgetControl->handle,
        address,
        led_colours[preset],
        on ? LV2_HMI_LED_Brightness_Normal : LV2_HMI_LED_Brightness_None
    );

    hmi->widgetControl->set_label(hmi->widgetControl->handle, address, message);
}

/** @p value: the name of the preset, shown as the value (NULL: the device shows its own) */
static void show_select(HMI* hmi, unsigned int current_preset, const char* value) {
    const LV2_HMI_Addressing address = hmi->select;

    if (hmi->widgetControl == NULL || address == NULL) {
        return;
    }

    hmi->widgetControl->set_led_with_brightness(
        hmi->widgetControl->handle,
        address,
        led_colours[current_preset],
        LV2_HMI_LED_Brightness_Normal
    );

    char message[8];
    snprintf(message, sizeof(message), "%u", current_preset + 1);

    hmi->widgetControl->set_label(hmi->widgetControl->handle, address, message);

    if (value != NULL && hmi->widgetControl->set_value != NULL) {
        hmi->widgetControl->set_value(hmi->widgetControl->handle, address, value);
    }
}

static void show_popups(HMI* hmi, const char* label) {
    for (unsigned int i=0; i<TOTAL_CONTROLLER_NOTIFIERS; i++) {
        const LV2_HMI_Addressing address = hmi->notifiers[i];

        if (address == NULL) {
            continue;
        }

        hmi->widgetControl->popup_message(
            hmi->widgetControl->handle,
            address,
            LV2_HMI_Popup_Style_Inverted,
            PLUGIN_NAME,
            label
        );
    }
}
