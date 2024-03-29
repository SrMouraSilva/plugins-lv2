#include <stdlib.h>
#include <stdbool.h>

#include <lv2/core/lv2.h>
#include <lv2/core/lv2_util.h>
#include <lv2/lv2plug.in/ns/ext/log/log.h>

#include "../extension/lv2-hmi.h"
#include "../../model/gossiper.h"
#include "lv2-hmi-gossiper.h"


bool LV2_HMI_Gossiper_is_extension_data_appliable(const char* uri) {
  return (!strcmp(uri, LV2_HMI__PluginNotification));
}


static void addressed(LV2_Handle handle, uint32_t index, LV2_HMI_Addressing addressing, const LV2_HMI_AddressingInfo* info);
static void unaddressed(LV2_Handle handle, uint32_t index);
static void assign(Gossiper* this, PortIndex index, LV2_HMI_Addressing addressing);

const void* LV2_HMI_Gossiper_extension_data() {
    static const LV2_HMI_PluginNotification hmiNotif = {
        addressed,
        unaddressed,
    };

    return &hmiNotif;
}

static void addressed(LV2_Handle handle, uint32_t index, LV2_HMI_Addressing addressing, const LV2_HMI_AddressingInfo* info) {
    Gossiper* this = (Gossiper*) handle;
    
    assign(this, (PortIndex) index, addressing);
}

static void unaddressed(LV2_Handle handle, uint32_t index) {
    Gossiper* this = (Gossiper*) handle;

    assign(this, (PortIndex) index, NULL);
}

void assign(Gossiper* this, PortIndex index, LV2_HMI_Addressing addressing) {
    unsigned int i = index - ASSIGN_TO_NOTIFY_1;

    switch (index) {
        case ASSIGN_TO_NOTIFY_1:
        case ASSIGN_TO_NOTIFY_2:
        case ASSIGN_TO_NOTIFY_3:
        case ASSIGN_TO_NOTIFY_4:
            this->notifiers[i].hmi_addressing = addressing;
            break;
    }
}


void run_notification(Gossiper* this);

void LV2_HMI_Gossiper_run(Gossiper* this) {
    run_notification(this);
}


void notify_footswithes_updated(Gossiper* gossiper, LV2_HMI_Addressing addressing);
void notify_potentiometers_updated(Gossiper* gossiper, LV2_HMI_Addressing addressing);

void run_notification(Gossiper* this) {
    for (unsigned int notifier=0; notifier<TOTAL_GOSSIPER_NOTIFIERS; notifier++) {
        LV2_HMI_Addressing addressing = this->notifiers[notifier].hmi_addressing;

        if (addressing == NULL) {
            continue;
        }

        notify_footswithes_updated(this, addressing);
        notify_potentiometers_updated(this, addressing);
    }
}

void notify_footswithes_updated(Gossiper* gossiper, LV2_HMI_Addressing addressing) {
    HMI* hmi = &gossiper->lv2->hmi;

    for (unsigned int i=0; i<TOTAL_GOSSIPER_FOOTSWITCHES; i++) {
        Switch footswitch = gossiper->switches[i];

        if (!footswitch.updated) {
            continue;
        }

        char message[14];
        sprintf(message, "Footswitch %d", i+1);

        hmi->widgetControl->popup_message(
            hmi->widgetControl->handle,
            addressing,
            footswitch.activated ? LV2_HMI_Popup_Style_Inverted : LV2_HMI_Popup_Style_Normal,
            "Gossiper",
            message
        );

        // If more than one notifiers is updated at the same time,
        // only shows the lowest 
        break;
    }
}

void notify_potentiometers_updated(Gossiper* gossiper, LV2_HMI_Addressing addressing) {
    HMI* hmi = &gossiper->lv2->hmi;

    for (unsigned int i=0; i<TOTAL_GOSSIPER_POTENTIOMETER; i++) {
        Potentiometer potentiometer = gossiper->potentiometers[i];

        bool show_message = potentiometer.updated && potentiometer.delay == 0;
        if (!show_message) {
            continue;
        }

        //lv2_log_error(&gossiper->lv2->logger, "Updated pot %d value: <%f>\n", i, potentiometer.value);

        char title[18];
        sprintf(title, "Gossiper - Pot %d", i+1);

        char message[6];
        sprintf(message, "%3.2f%%", potentiometer.value * 100);

        hmi->widgetControl->popup_message(
            hmi->widgetControl->handle,
            addressing,
            LV2_HMI_Popup_Style_Normal,
            title,
            message
        );

        // If more than one notifiers is updated at the same time,
        // only shows the lowest 
        break;
    }
}