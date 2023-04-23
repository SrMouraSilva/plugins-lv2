#ifndef TETR4_SWITCH_H
#define	TETR4_SWITCH_H

/** Total of presets. The number of footswitches is also related */
//#define TOTAL_PRESETS 4
/** How many CV outputs there are */
//#define TOTAL_OUTPUTS 4

#define PLUGIN_URI "http://srmourasilva.github.io/plugins/tetr4-switch"


typedef enum {
    // Outputs
    OUTPUT_CV_1 = 0,
    OUTPUT_CV_2 = 1,
    OUTPUT_CV_3 = 2,
    OUTPUT_CV_4 = 3,

    // Preset selectors (footswitch selectors)
    PRESET_SELECTOR_1 = 4,
    PRESET_SELECTOR_2 = 5,
    PRESET_SELECTOR_3 = 6,
    PRESET_SELECTOR_4 = 7,

    PRESET_SELECT = 8,
    ASSIGN_TO_NOTIFY = 9,

    // DIP Switches
    PRESET_1_OUTPUT_1 = 10,
    PRESET_1_OUTPUT_2 = 11,
    PRESET_1_OUTPUT_3 = 12,
    PRESET_1_OUTPUT_4 = 13,

    PRESET_2_OUTPUT_1 = 14,
    PRESET_2_OUTPUT_2 = 15,
    PRESET_2_OUTPUT_3 = 16,
    PRESET_2_OUTPUT_4 = 17,

    PRESET_3_OUTPUT_1 = 18,
    PRESET_3_OUTPUT_2 = 19,
    PRESET_3_OUTPUT_3 = 20,
    PRESET_3_OUTPUT_4 = 21,

    PRESET_4_OUTPUT_1 = 22,
    PRESET_4_OUTPUT_2 = 23,
    PRESET_4_OUTPUT_3 = 24,
    PRESET_4_OUTPUT_4 = 25,

    EVENTS_IN = 26,
} PortIndex;

#endif