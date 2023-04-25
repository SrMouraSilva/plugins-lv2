#ifndef GOSSIPER_H
#define	GOSSIPER_H

#define PLUGIN_URI "http://srmourasilva.github.io/plugins/gossiper"

#define TOTAL_GOSSIPER_FOOTSWITCHES 4
#define TOTAL_GOSSIPER_NOTIFIERS 4

#define MAX_TENSION 10

#include "../lv2/extension/lv2-hmi.h"
#include "../lv2/gossiper/lv2-gossiper.h"


typedef enum {
    // Outputs
    OUTPUT_CV_1 = 0,
    OUTPUT_CV_2 = 1,
    OUTPUT_CV_3 = 2,
    OUTPUT_CV_4 = 3,

    // Preset selectors (footswitch selectors)
    FOOTSWITCH_1 = 4,
    FOOTSWITCH_2 = 5,
    FOOTSWITCH_3 = 6,
    FOOTSWITCH_4 = 7,

    ASSIGN_TO_NOTIFY_1 = 8,
    ASSIGN_TO_NOTIFY_2 = 9,
    ASSIGN_TO_NOTIFY_3 = 10,
    ASSIGN_TO_NOTIFY_4 = 11,

    //EVENTS_IN = 12,
} PortIndex;

// -----------------------------------------------

typedef struct {
    /** Is switch activated/on? */
    bool activated;

    /** Preset changed at this cycle */
    bool updated;

    /** LV2 input */
    float* input;
} Switch;

typedef struct {
    /** LV2 input */
    float* input;

    LV2_HMI_Addressing* hmi_addressing;
} Notifier;

typedef struct {
    float* output;
} CvOutput;

typedef struct {
    // Attributes
    Switch switches[TOTAL_GOSSIPER_FOOTSWITCHES];
    CvOutput outputs[TOTAL_GOSSIPER_FOOTSWITCHES];
    Notifier notifiers[TOTAL_GOSSIPER_NOTIFIERS];


    LV2_Gossiper* lv2;

    /**
     * Get the label of the specified preset or NULL if send a invalid preset
     * 
     * @param void * Gossiper
     * @param index: Index of the preset
     * 
     * @return Label of the specified preset or NULL if index is invalid
     */
    //char* (* get_preset_label)(void* self, unsigned int index);

    /**
     * Rename the label of the specified preset
     * 
     * @param void * Gossiper
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
     * @param void * Gossiper
     * 
     * Run just once at cycle
     */
    void (* run)(void* self, uint32_t n_samples);

} Gossiper;


extern Gossiper* Gossiper_instantiate();

#endif