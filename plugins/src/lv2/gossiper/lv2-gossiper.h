#ifndef LV2_GOSSIPER_H
#define	LV2_GOSSIPER_H

#include "lv2/log/logger.h"
#include "lv2/atom/atom.h"
#include "lv2/urid/urid.h"

#include "../extension/lv2-hmi.h"

#include "../../config.h"

#define GOSSIPER_PLUGIN_URI "http://srmourasilva.github.io/plugins/gossiper"
#define GOSSIPER_FOOTSWITCH_LABEL_COUNT 7

typedef struct {
    LV2_HMI_WidgetControl* widgetControl;
} HMI;


typedef struct {
    LV2_URID atom_Object;
    LV2_URID atom_URID;
    LV2_URID atom_String;
    LV2_URID patch_Set;
    LV2_URID patch_property;
    LV2_URID patch_value;
    LV2_URID footswitch_label[GOSSIPER_FOOTSWITCH_LABEL_COUNT];
} Gossiper_URIs;

typedef struct {
    LV2_URID_Map* map;

    const LV2_Atom_Sequence* events_in;

    Gossiper_URIs uris;

    // Capabilities

    /** LV2 Logging */
    LV2_Log_Logger logger;

    /** Mod HMI */
    HMI hmi;

    /**
     * Initialize the lv2 capabilities
     * 
     * @param void * LV2_Gossiper
     */
    const char* (* initialize)(void* self, const LV2_Feature* const* features);

} LV2_Gossiper;


extern LV2_Gossiper* LV2_Gossiper_instantiate();

#endif