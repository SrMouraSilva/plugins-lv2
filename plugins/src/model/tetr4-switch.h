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

    ASSIGN_TO_NOTIFY_1 = 9,
    ASSIGN_TO_NOTIFY_2 = 10,
    ASSIGN_TO_NOTIFY_3 = 11,
    ASSIGN_TO_NOTIFY_4 = 12,

    // DIP Switches
    PRESET_1_OUTPUT_1 = 13,
    PRESET_1_OUTPUT_2 = 14,
    PRESET_1_OUTPUT_3 = 15,
    PRESET_1_OUTPUT_4 = 16,

    PRESET_2_OUTPUT_1 = 17,
    PRESET_2_OUTPUT_2 = 18,
    PRESET_2_OUTPUT_3 = 19,
    PRESET_2_OUTPUT_4 = 20,

    PRESET_3_OUTPUT_1 = 21,
    PRESET_3_OUTPUT_2 = 22,
    PRESET_3_OUTPUT_3 = 23,
    PRESET_3_OUTPUT_4 = 24,

    PRESET_4_OUTPUT_1 = 25,
    PRESET_4_OUTPUT_2 = 26,
    PRESET_4_OUTPUT_3 = 27,
    PRESET_4_OUTPUT_4 = 28,

    EVENTS_IN = 29,
} PortIndex;

#endif