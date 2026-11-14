/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LV2_CONTROLLER_H
#define	LV2_CONTROLLER_H

#include <stdbool.h>

#include "lv2/log/logger.h"
#include "lv2/atom/atom.h"
#include "lv2/atom/forge.h"
#include "lv2/urid/urid.h"
#include "lv2/worker/worker.h"

#include "../extension/lv2-hmi.h"
#include "../extension/control-input-port-change-request.h"

#include "../../config.h"

typedef struct {
    LV2_HMI_WidgetControl* widgetControl;

    LV2_HMI_Addressing preset[TOTAL_PRESETS];
    LV2_HMI_Addressing select;

    LV2_HMI_Addressing notifiers[TOTAL_CONTROLLER_NOTIFIERS];
} HMI;

typedef struct {
    LV2_URID atom_Object;
    LV2_URID atom_URID;
    LV2_URID atom_String;
    LV2_URID patch_Set;
    LV2_URID patch_Get;
    LV2_URID patch_property;
    LV2_URID patch_value;
    LV2_URID preset_label[TOTAL_PRESETS];
} Controller_URIs;

typedef struct {
    LV2_URID_Map* map;

    const LV2_Atom_Sequence* events_in;
    LV2_Atom_Sequence* events_out;

    /** Used to write patch:Set replies to events_out */
    LV2_Atom_Forge forge;

    /**
     * Set by state restore (e.g. loading a preset) when it changed the preset labels;
     * the next run() sends them on events_out so an open GUI shows the new labels.
     */
    bool labels_restored;

    Controller_URIs uris;

    // Capabilities

    /** LV2 Logging */
    LV2_Log_Logger logger;

    /** Mod HMI */
    HMI hmi;

    /** Request change */
    LV2_ControlInputPort_Change_Request* control_input_port;

    /**
     * LV2 worker (optional): the messages to the HMI are sent from its thread, not from run(), as the host locks a
     * mutex and writes to the HMI channel for each of them. Without it they are sent from run()
     */
    LV2_Worker_Schedule* worker;


    /**
     * Initialize the lv2 capabilities
     *
     * @param void * LV2_Controller
     */
    const char* (* initialize)(void* self, const LV2_Feature* const* features);

} LV2_Controller;


extern LV2_Controller* LV2_Controller_instantiate();

#endif