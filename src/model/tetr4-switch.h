#ifndef TETR4_SWITCH_H
#define	TETR4_SWITCH_H

#include "lv2/log/logger.h"

/** Total of presets. The number of footswitches is also related */
#define TOTAL_PRESETS 4
/** How many CV outputs there are */
#define TOTAL_OUTPUTS 4
/** Tension when is on */
#define MAX_TENSION 10 // volts
#define PRESET_LABEL_SIZE 11

typedef struct {
    char preset_label_1[PRESET_LABEL_SIZE];
    char preset_label_2[PRESET_LABEL_SIZE];
    char preset_label_3[PRESET_LABEL_SIZE];
    char preset_label_4[PRESET_LABEL_SIZE];
} State;

typedef struct {
    // Attributes
    float* output_cvs[TOTAL_PRESETS];
    bool* preset_selectors[TOTAL_PRESETS];

    bool* preset_outputs[TOTAL_PRESETS][TOTAL_OUTPUTS];

    bool* inverters[TOTAL_OUTPUTS];
    char* preset_labels[TOTAL_PRESETS];

    // State
    State state;

    // Features
    LV2_HMI_Addressing hmi_led_addressing;
    LV2_HMI_Addressing hmi_label_addressing;

    LV2_HMI_WidgetControl* hmi;
    LV2_Log_Logger logger;
    LV2_URID_Map* map;

    /**
     * Get the index of current preset.
     * Note: The first corresponds to '0' (zero)
     * 
     * @param void * Tetr4Switch
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
} Tetr4Switch;


extern void Tetr4Switch_instantiate(Tetr4Switch* self);

#endif