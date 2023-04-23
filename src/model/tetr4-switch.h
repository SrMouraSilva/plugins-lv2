#ifndef TETR4_SWITCH_H
#define	TETR4_SWITCH_H

#include <stdbool.h>

#include "lv2/log/logger.h"
#include "lv2/atom/atom.h"

#include "../extension/lv2-hmi.h"
#include "../extension/control-input-port-change-request.h"


#define PLUGIN_URI "http://srmourasilva.github.io/plugins/tetr4-switch"


/** Total of presets. The number of footswitches is also related */
#define TOTAL_PRESETS 4
/** How many CV outputs there are */
#define TOTAL_OUTPUTS 4
/** Tension when is on */
#define MAX_TENSION 10 // volts
#define PRESET_LABEL_MAX_SIZE 11

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
    // Attributes
    float* output_cvs[TOTAL_PRESETS];
    float* preset_selectors[TOTAL_PRESETS];

    float* current_preset_index;

    float* preset_outputs[TOTAL_PRESETS][TOTAL_OUTPUTS];

    float* inverters[TOTAL_OUTPUTS];

    /** Preset changed at this cycle */
    bool preset_changed;
    /** Preset activated on previous cycle */
    unsigned int previous_preset_mask;
    unsigned int current_preset_mask;

    // Atom feature - Preset labels
    const LV2_Atom_Sequence* events_in;

    // State
    URIs uris;
    State state;

    // Features
    LV2_URID_Map* map;
    LV2_Log_Logger logger;

    LV2_HMI_WidgetControl* hmi;

    LV2_HMI_Addressing hmi_preset_addressing[TOTAL_PRESETS];
    LV2_HMI_Addressing hmi_combobox_addressing;
    LV2_HMI_Addressing hmi_notification_addressing;
    // LV2_HMI_Addressing hmi_led_addressing;
    // LV2_HMI_Addressing hmi_label_addressing;

    LV2_ControlInputPort_Change_Request* control_input_port;

    /**
     * Calculate necessary internal operations
     * 
     * @param void * Tetr4Switch
     * 
     * Run just once at cycle
     */
    void (* run)(void* self);

    /**
     * Calculate the index of previous preset
     * 
     * If self->preset_changed == true, previous preset != current preset
     * else previous preset == current preset
     * 
     * Note: The first corresponds to '0' (zero)
     * 
     * @param void * Tetr4Switch
     * 
     * @return Index of previous preset
     */
    unsigned int (* get_index_previous_preset)(void* self);

    /**
     * Calculate the index of current preset
     * Note: The first corresponds to '0' (zero)
     * 
     * @param void * Tetr4Switch
     * 
     * @return Index of current preset
     */
    unsigned int (* get_index_current_preset)(void* self);

    /**
     * Get the current preset as the masked way:
     *   The least significative bit corresponds to the first plugin
     *   The seconds least significative bit corresponds to the second plugin
     *   and so on
     * 
     * @param void * Tetr4Switch
     * 
     * @return Current preset as the masked way
     */
    unsigned int (* get_current_preset)(void* self);

    /**
     * Get the inverter as the masked way:
     *   The least significative bit corresponds to the first output inverter
     *   The seconds least significative bit corresponds to the second output inverter
     *   and so on
     * 
     * The inverter state is unique: doesn't changes with the preset states
     * 
     * @param void * Tetr4Switch
     * 
     * @return Current preset as the masked way
     */
    unsigned int (* get_inverter)(void* self);

    /**
     * Get the output signal as the masked way:
     *   The least significative bit corresponds to the first output signal
     *   The seconds least significative bit corresponds to the second output signal
     *   and so on
     * 
     * The inverter state is unique: doesn't changes with the preset states
     * 
     * @param void * Tetr4Switch
     * 
     * @return Current preset as the masked way
     */
    unsigned int (* get_output_signal)(void* self);

    /**
     * Get the label of the specified preset or NULL if send a invalid preset
     * 
     * @param void * Tetr4Switch
     * @param index: Index of the preset
     * 
     * @return Label of the specified preset or NULL if index is invalid
     */
    char* (* get_preset_label)(void* self, unsigned int index);

    /**
     * Rename the label of the specified preset
     * 
     * @param void * Tetr4Switch
     * @param index Index of the patch where will be renamed
     * @param new_label New label with max size PRESET_LABEL_MAX_SIZE-1
     * 
     * @return Current preset as the masked way
     */
    char* (* set_preset_label)(void* self, unsigned int index, char* new_label);
} Tetr4Switch;


extern void Tetr4Switch_instantiate(Tetr4Switch* self);

#endif