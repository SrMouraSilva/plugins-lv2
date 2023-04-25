#ifndef CONTROLLER_H
#define	CONTROLLER_H

#include <stdbool.h>
#include "../config.h"

#include "../lv2/controller/lv2-controller.h"

/** Tension when is on */
#define MAX_TENSION 10 // volts


typedef struct {
    /** Preset changed at this cycle */
    bool preset_changed;

    /** Preset activated on previous cycle */
    unsigned int previous_preset_mask;

    unsigned int current_preset_mask;    
} InternalState;


typedef struct {
    // Attributes
    float* output_cvs[TOTAL_PRESETS];
    float* preset_selectors[TOTAL_PRESETS];

    float* current_preset_index;

    float* preset_outputs[TOTAL_PRESETS][TOTAL_OUTPUTS];

    float* inverters[TOTAL_OUTPUTS];

    InternalState internal_state;

    LV2_Controller* lv2;

    /**
     * @param void * Controller
     * 
     * @return Is preset changed on the current cycle?
     */
    bool (* is_preset_changed)(void* self);

    /**
     * Calculate the index of previous preset
     * 
     * If self->preset_changed == true, previous preset != current preset
     * else previous preset == current preset
     * 
     * Note: The first corresponds to '0' (zero)
     * 
     * @param void * Controller
     * 
     * @return Index of previous preset
     */
    unsigned int (* get_index_previous_preset)(void* self);

    /**
     * Calculate the index of current preset
     * Note: The first corresponds to '0' (zero)
     * 
     * @param void * Controller
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
     * @param void * Controller
     * 
     * @return Current preset as the masked way
     */
    unsigned int (* get_current_preset)(void* self);

    /**
     * Get the output signal as the masked way:
     *   The least significative bit corresponds to the first output signal
     *   The seconds least significative bit corresponds to the second output signal
     *   and so on
     * 
     * The inverter state is unique: doesn't changes with the preset states
     * 
     * @param void * Controller
     * 
     * @return Current preset as the masked way
     */
    unsigned int (* get_output_signal)(void* self);

    /**
     * Get the label of the specified preset or NULL if send a invalid preset
     * 
     * @param void * Controller
     * @param index: Index of the preset
     * 
     * @return Label of the specified preset or NULL if index is invalid
     */
    //char* (* get_preset_label)(void* self, unsigned int index);

    /**
     * Rename the label of the specified preset
     * 
     * @param void * Controller
     * @param index Index of the patch where will be renamed
     * @param new_label New label with max size PRESET_LABEL_MAX_SIZE-1
     * 
     * @return Current preset as the masked way
     */
    //char* (* set_preset_label)(void* self, unsigned int index, char* new_label);

    /**
     * Calculate necessary internal operations
     * and update the output values
     * 
     * @param void * Controller
     * 
     * Run just once at cycle
     */
    void (* run)(void* self, uint32_t n_samples);

} Controller;


extern Controller* Controller_instantiate();

#endif