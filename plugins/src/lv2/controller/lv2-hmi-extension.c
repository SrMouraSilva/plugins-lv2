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
 * How much of a preset's name fits on the label of a footswitch of the Dwarf: 7 characters (tried on a device; an enumeration
 * shares them with its value, so it has less)
 */
#define HMI_NAME_MAX_LENGTH 7

/**
 * What run() hands to the worker: a change of preset, a change of names, or both. The names are copied: run() may
 * change them (a patch:Set) while the worker is still sending them
 */
typedef struct {
    uint32_t current_preset;
    uint32_t previous_preset;
    /** "Current preset" goes through a cycle: the device's own value for it may not be the active preset */
    uint32_t show_value;
    /** The active preset changed: its LEDs, the select and the pop-ups */
    uint32_t preset_changed;
    /** A name or the cycle changed: the labels of the widgets are sent again */
    uint32_t labels_changed;
    /** Where "Next preset" and "Previous preset" lead (Hept7 Switch) */
    uint32_t next_target;
    uint32_t previous_target;
    char labels[TOTAL_PRESETS][PRESET_LABEL_MAX_SIZE];
} HMI_PresetChange;


static void LV2_HMI_addressed(LV2_Handle handle, uint32_t index, LV2_HMI_Addressing addressing, const LV2_HMI_AddressingInfo* info);
static void LV2_HMI_unaddressed(LV2_Handle handle, uint32_t index);
static void LV2_HMI_assign(Controller* self, uint32_t index, LV2_HMI_Addressing addressing, unsigned int caps);
static void LV2_HMI_send_preset_change(Controller* self, const HMI_PresetChange* change);

static void format_display_name(char* message, size_t size, const char* name);
static void show_footswitch_led(HMI* hmi, unsigned int preset, bool on);
static void show_footswitch_label(HMI* hmi, unsigned int preset, const char* name);
static void show_select(HMI* hmi, unsigned int current_preset, const char* name, bool name_as_value);
static void show_target(HMI* hmi, LV2_HMI_Addressing address, unsigned int caps, unsigned int target, const char* name);
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
    Controller* self = (Controller*) handle;
    unsigned int caps = 0;

    if (info != NULL) {
        caps = (unsigned int) info->caps;

        // The B+C pair (two footswitches stepping an enumeration) is the addressing the device reports as "reverse"
        // without being momentary (observed on the Dwarf: a single footswitch is 0, a momentary one 0x2, the pair 0x4).
        // The firmware lights both LEDs and writes the text of such a pair itself: a colour or a label from here only
        // fights with it
        if ((info->flags & LV2_HMI_AddressingFlag_Reverse) && !(info->flags & LV2_HMI_AddressingFlag_Momentary)) {
            caps &= ~(LV2_HMI_AddressingCapability_LED | LV2_HMI_AddressingCapability_Label | LV2_HMI_AddressingCapability_Value);
        }
    }

    LV2_HMI_assign(self, index, addressing, caps);
}

static void LV2_HMI_unaddressed(LV2_Handle handle, uint32_t index) {
    LV2_HMI_assign((Controller*) handle, index, NULL, 0);
}

/**
 * Keep the addressing, and show the state on the widget that has just been addressed (and only on it).
 * A notifier shows nothing here: a pop-up is only for a change of preset (otherwise loading a pedalboard with the
 * notifiers addressed would show one pop-up per notifier, one after the other)
 */
static void LV2_HMI_assign(Controller* self, uint32_t index, LV2_HMI_Addressing addressing, unsigned int caps) {
    HMI* hmi = &self->lv2->hmi;
    const unsigned int current_preset = self->current_preset;

    if (index >= PORT_PRESET_SELECTOR_FIRST && index < PORT_PRESET_SELECT) {
        const unsigned int preset = index - PORT_PRESET_SELECTOR_FIRST;

        hmi->preset[preset] = addressing;
        hmi->preset_caps[preset] = caps;
        show_footswitch_led(hmi, preset, preset == current_preset);
        show_footswitch_label(hmi, preset, self->preset_labels[preset]);

    } else if (index == PORT_PRESET_SELECT) {
        hmi->select = addressing;
        hmi->select_caps = caps;
        show_select(hmi, current_preset, self->preset_labels[current_preset], Controller_has_cycle(self));

    } else if (index >= PORT_ASSIGN_TO_NOTIFY_FIRST && index < PORT_PRESET_OUTPUT_FIRST) {
        hmi->notifiers[index - PORT_ASSIGN_TO_NOTIFY_FIRST] = addressing;

#ifdef HEPT7_SWITCH
    } else if (index == PORT_NEXT_PRESET) {
        const unsigned int target = Controller_cycle_target(self, 1);

        hmi->next = addressing;
        hmi->next_caps = caps;
        show_target(hmi, addressing, caps, target, self->preset_labels[target]);

    } else if (index == PORT_PREVIOUS_PRESET) {
        const unsigned int target = Controller_cycle_target(self, -1);

        hmi->previous = addressing;
        hmi->previous_caps = caps;
        show_target(hmi, addressing, caps, target, self->preset_labels[target]);
#endif
    }
}


void LV2_HMI_run(Controller* self) {
    // The device draws its own text for a footswitch it has just pressed, and that has to be drawn over
    if (self->lv2->hmi.widgetControl == NULL || (!self->preset_changed && !self->labels_changed && !self->combobox_moved)) {
        return;
    }

    HMI_PresetChange change;
    change.current_preset = self->current_preset;
    change.previous_preset = self->previous_preset;
    change.show_value = Controller_has_cycle(self);
    change.preset_changed = self->preset_changed;
    change.labels_changed = self->labels_changed;
    change.next_target = Controller_cycle_target(self, 1);
    change.previous_target = Controller_cycle_target(self, -1);
    memcpy(change.labels, self->preset_labels, sizeof(change.labels));

    self->labels_changed = false;

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

/**
 * Only what the change touches. A change of preset: the footswitch turned off, the one turned on, the select, the
 * pop-ups. A change of names: the label of every footswitch, and the select
 */
static void LV2_HMI_send_preset_change(Controller* self, const HMI_PresetChange* change) {
    HMI* hmi = &self->lv2->hmi;

    if (hmi->widgetControl == NULL
        || change->current_preset >= TOTAL_PRESETS
        || change->previous_preset >= TOTAL_PRESETS) {
        return;
    }

    if (change->preset_changed) {
        if (change->previous_preset != change->current_preset) {
            show_footswitch_led(hmi, change->previous_preset, false);
        }
        show_footswitch_led(hmi, change->current_preset, true);
    }

    if (change->labels_changed) {
        for (unsigned int preset=0; preset<TOTAL_PRESETS; preset++) {
            show_footswitch_label(hmi, preset, change->labels[preset]);
        }
    }

    show_select(hmi, change->current_preset, change->labels[change->current_preset], change->show_value);

    // Where they lead: after a change of preset, of a name or of the cycle
    if ((change->preset_changed || change->labels_changed) && change->next_target < TOTAL_PRESETS && change->previous_target < TOTAL_PRESETS) {
        show_target(hmi, hmi->next, hmi->next_caps, change->next_target, change->labels[change->next_target]);
        show_target(hmi, hmi->previous, hmi->previous_caps, change->previous_target, change->labels[change->previous_target]);
    }

    if (change->preset_changed) {
        show_popups(hmi, change->labels[change->current_preset]);
    }
}


static void show_footswitch_led(HMI* hmi, unsigned int preset, bool on) {
    const LV2_HMI_Addressing address = hmi->preset[preset];

    if (hmi->widgetControl == NULL || address == NULL || !(hmi->preset_caps[preset] & LV2_HMI_AddressingCapability_LED)) {
        return;
    }

    hmi->widgetControl->set_led_with_brightness(
        hmi->widgetControl->handle,
        address,
        led_colours[preset],
        on ? LV2_HMI_LED_Brightness_Normal : LV2_HMI_LED_Brightness_None
    );
}

/**
 * A preset's name as the display has room for it: the first HMI_NAME_MAX_LENGTH characters. The default names ("Preset 3")
 * would all be cut to "Preset ", so they are written "Prst 3"
 */
static void format_display_name(char* message, size_t size, const char* name) {
    if (!strncmp(name, "Preset ", 7)) {
        snprintf(message, size, "Prst %.*s", HMI_NAME_MAX_LENGTH - 5, name + 7);
    } else {
        snprintf(message, size, "%.*s", HMI_NAME_MAX_LENGTH, name);
    }
}

/** The name of the preset, as much as fits: the state is the LED (and the device inverts what is on) */
static void show_footswitch_label(HMI* hmi, unsigned int preset, const char* name) {
    const LV2_HMI_Addressing address = hmi->preset[preset];

    if (hmi->widgetControl == NULL || address == NULL || !(hmi->preset_caps[preset] & LV2_HMI_AddressingCapability_Label)) {
        return;
    }

    char message[HMI_NAME_MAX_LENGTH + 1];
    format_display_name(message, sizeof(message), name);

    hmi->widgetControl->set_label(hmi->widgetControl->handle, address, message);
}

/**
 * The widget of "Current preset": the colour of the active preset and its name. A widget that shows a value of its
 * own (a knob) keeps the number as the label and takes the name as the value, when @p name_as_value (the device's
 * own value is not the active preset); one that does not (a footswitch) has the name as its label
 */
static void show_select(HMI* hmi, unsigned int current_preset, const char* name, bool name_as_value) {
    const LV2_HMI_Addressing address = hmi->select;

    if (hmi->widgetControl == NULL || address == NULL) {
        return;
    }

    const unsigned int caps = hmi->select_caps;
    const bool has_value = (caps & LV2_HMI_AddressingCapability_Value) && hmi->widgetControl->set_value != NULL;

    if (caps & LV2_HMI_AddressingCapability_LED) {
        hmi->widgetControl->set_led_with_brightness(
            hmi->widgetControl->handle,
            address,
            led_colours[current_preset],
            LV2_HMI_LED_Brightness_Normal
        );
    }

    if (caps & LV2_HMI_AddressingCapability_Label) {
        char message[HMI_NAME_MAX_LENGTH + 1];

        if ((caps & LV2_HMI_AddressingCapability_Value)) {
            snprintf(message, sizeof(message), "%u", current_preset + 1);
        } else {
            format_display_name(message, sizeof(message), name);
        }

        hmi->widgetControl->set_label(hmi->widgetControl->handle, address, message);
    }

    if (name_as_value && has_value) {
        hmi->widgetControl->set_value(hmi->widgetControl->handle, address, name);
    }
}

/**
 * "Next preset" and "Previous preset": the name of the preset they lead to, and its colour dimmed (the active preset's
 * own colour is the bright one, on its selector and on "Current preset")
 */
static void show_target(HMI* hmi, LV2_HMI_Addressing address, unsigned int caps, unsigned int target, const char* name) {
    if (hmi->widgetControl == NULL || address == NULL) {
        return;
    }

    if (caps & LV2_HMI_AddressingCapability_LED) {
        hmi->widgetControl->set_led_with_brightness(
            hmi->widgetControl->handle,
            address,
            led_colours[target],
            LV2_HMI_LED_Brightness_Low
        );
    }

    if (caps & LV2_HMI_AddressingCapability_Label) {
        char message[HMI_NAME_MAX_LENGTH + 1];
        format_display_name(message, sizeof(message), name);

        hmi->widgetControl->set_label(hmi->widgetControl->handle, address, message);
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
