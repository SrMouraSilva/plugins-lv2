#ifndef LV2_CONTROLLER_H
#define	LV2_CONTROLLER_H

#include "lv2/log/logger.h"
#include "lv2/atom/atom.h"
#include "lv2/atom/forge.h"
#include "lv2/urid/urid.h"

#include "../extension/lv2-hmi.h"
#include "../extension/control-input-port-change-request.h"

#include "../../config.h"

#define TETR4_SWITCH_PLUGIN_URI "http://srmourasilva.github.io/plugins/tetr4-switch"

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

    Controller_URIs uris;

    // Capabilities

    /** LV2 Logging */
    LV2_Log_Logger logger;

    /** Mod HMI */
    HMI hmi;

    /** Request change */
    LV2_ControlInputPort_Change_Request* control_input_port;


    /**
     * Initialize the lv2 capabilities
     *
     * @param void * LV2_Controller
     */
    const char* (* initialize)(void* self, const LV2_Feature* const* features);

} LV2_Controller;


extern LV2_Controller* LV2_Controller_instantiate();

#endif