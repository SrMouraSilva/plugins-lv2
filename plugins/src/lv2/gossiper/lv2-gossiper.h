#ifndef LV2_GOSSIPER_H
#define	LV2_GOSSIPER_H

#include "lv2/log/logger.h"
#include "lv2/atom/atom.h"

#include "../extension/lv2-hmi.h"

#include "../../config.h"

typedef struct {
    LV2_HMI_WidgetControl* widgetControl;
} HMI;


// typedef struct {
//     // Atom feature - Preset labels
//     const LV2_Atom_Sequence* events_in;

//     // State
//     URIs uris;
//     State state;
// } Controller_Atom;

typedef struct {
    LV2_URID_Map* map;

    // Capabilities

    /** LV2 Logging */
    LV2_Log_Logger logger;

    /** Mod HMI */
    HMI hmi;

    /** Atom */
    // Controller_Atom atom;


    /**
     * Initialize the lv2 capabilities
     * 
     * @param void * LV2_Gossiper
     */
    const char* (* initialize)(void* self, const LV2_Feature* const* features);

} LV2_Gossiper;


extern LV2_Gossiper* LV2_Gossiper_instantiate();

#endif