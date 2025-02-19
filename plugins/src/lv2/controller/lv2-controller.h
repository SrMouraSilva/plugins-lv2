#ifndef LV2_CONTROLLER_H
#define	LV2_CONTROLLER_H

#include "lv2/log/logger.h"
#include "lv2/atom/atom.h"
#include "lv2/midi/midi.h"

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
    // Types
    //LV2_URID atom_String;

    // Values
    LV2_URID atom_presets_label[TOTAL_PRESETS];
} URIs;

typedef struct {
    char presets_label[TOTAL_PRESETS][PRESET_LABEL_MAX_SIZE];
} State;


typedef struct {
    // Atom feature - Preset labels
    const LV2_Atom_Sequence* events_in;

    // State
    
    State state;
} Controller_Atom;

typedef struct {
    LV2_URID atom_Sequence;
    LV2_URID midi_Event;
} MIDI_URIs;


static inline void
map_midi_uris( LV2_URID_Map* map, MIDI_URIs* uris)
{
  uris->atom_Sequence      = map->map(map->handle, LV2_ATOM__Sequence);
  uris->midi_Event         = map->map(map->handle, LV2_MIDI__MidiEvent);
}



typedef struct {
    LV2_URID_Map* map;

    // Capabilities

    /** LV2 Logging */
    LV2_Log_Logger logger;

    MIDI_URIs uris;

    /** Mod HMI */
    HMI hmi;

    /** Request change */
    LV2_ControlInputPort_Change_Request* control_input_port;

    /** Atom */
    Controller_Atom atom;


    /**
     * Initialize the lv2 capabilities
     * 
     * @param void * LV2_Controller
     */
    const char* (* initialize)(void* self, const LV2_Feature* const* features);

} LV2_Controller;


extern LV2_Controller* LV2_Controller_instantiate();

#endif