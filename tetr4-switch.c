#include <stdbool.h>
#include <math.h>
#include <stdlib.h>

// #define bool unsigned int

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

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

    // DIP Switches
    PRESET_1_OUTPUT_1 = 8,
    PRESET_1_OUTPUT_2 = 9,
    PRESET_1_OUTPUT_3 = 10,
    PRESET_1_OUTPUT_4 = 11,

    PRESET_2_OUTPUT_1 = 12,
    PRESET_2_OUTPUT_2 = 13,
    PRESET_2_OUTPUT_3 = 14,
    PRESET_2_OUTPUT_4 = 15,

    PRESET_3_OUTPUT_1 = 16,
    PRESET_3_OUTPUT_2 = 17,
    PRESET_3_OUTPUT_3 = 18,
    PRESET_3_OUTPUT_4 = 19,

    PRESET_4_OUTPUT_1 = 20,
    PRESET_4_OUTPUT_2 = 21,
    PRESET_4_OUTPUT_3 = 22,
    PRESET_4_OUTPUT_4 = 23,

    // Inverters
    INVERTER_1 = 24,
    INVERTER_2 = 25,
    INVERTER_3 = 26,
    INVERTER_4 = 27,
} PortIndex;

typedef struct {
    float* output_cv_1;
    float* output_cv_2;
    float* output_cv_3;
    float* output_cv_4;

    bool* preset_selector_1;
    bool* preset_selector_2;
    bool* preset_selector_3;
    bool* preset_selector_4;

    bool* preset_1_output_1;
    bool* preset_1_output_2;
    bool* preset_1_output_3;
    bool* preset_1_output_4;

    bool* preset_2_output_1;
    bool* preset_2_output_2;
    bool* preset_2_output_3;
    bool* preset_2_output_4;

    bool* preset_3_output_1;
    bool* preset_3_output_2;
    bool* preset_3_output_3;
    bool* preset_3_output_4;

    bool* preset_4_output_1;
    bool* preset_4_output_2;
    bool* preset_4_output_3;
    bool* preset_4_output_4;


    bool* inverter_1;
    bool* inverter_2;
    bool* inverter_3;
    bool* inverter_4;
} Tetr4Switch;

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
    Tetr4Switch* self = (Tetr4Switch*) malloc(sizeof(Tetr4Switch));

    return (LV2_Handle) self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    Tetr4Switch* self = (Tetr4Switch*) instance;

    switch ((PortIndex) port) {
        case OUTPUT_CV_1:
            self->output_cv_1 = (float*) data; break;
        case OUTPUT_CV_2:
            self->output_cv_2 = (float*) data; break;
        case OUTPUT_CV_3:
            self->output_cv_3 = (float*) data; break;
        case OUTPUT_CV_4:
            self->output_cv_4 = (float*) data; break;
        
        case PRESET_SELECTOR_1:
            self->preset_selector_1 = (bool*) data; break;
        case PRESET_SELECTOR_2:
            self->preset_selector_2 = (bool*) data; break;
        case PRESET_SELECTOR_3:
            self->preset_selector_3 = (bool*) data; break;
        case PRESET_SELECTOR_4:
            self->preset_selector_4 = (bool*) data; break;

        case PRESET_1_OUTPUT_1:
            self->preset_1_output_1 = (bool*) data; break;
        case PRESET_1_OUTPUT_2:
            self->preset_1_output_2 = (bool*) data; break;
        case PRESET_1_OUTPUT_3:
            self->preset_1_output_3 = (bool*) data; break;
        case PRESET_1_OUTPUT_4:
            self->preset_1_output_4 = (bool*) data; break;

        case PRESET_2_OUTPUT_1:
            self->preset_2_output_1 = (bool*) data; break;
        case PRESET_2_OUTPUT_2:
            self->preset_2_output_2 = (bool*) data; break;
        case PRESET_2_OUTPUT_3:
            self->preset_2_output_3 = (bool*) data; break;
        case PRESET_2_OUTPUT_4:
            self->preset_2_output_4 = (bool*) data; break;

        case PRESET_3_OUTPUT_1:
            self->preset_3_output_1 = (bool*) data; break;
        case PRESET_3_OUTPUT_2:
            self->preset_3_output_2 = (bool*) data; break;
        case PRESET_3_OUTPUT_3:
            self->preset_3_output_3 = (bool*) data; break;
        case PRESET_3_OUTPUT_4:
            self->preset_3_output_4 = (bool*) data; break;

        case PRESET_4_OUTPUT_1:
            self->preset_4_output_1 = (bool*) data; break;
        case PRESET_4_OUTPUT_2:
            self->preset_4_output_2 = (bool*) data; break;
        case PRESET_4_OUTPUT_3:
            self->preset_4_output_3 = (bool*) data; break;
        case PRESET_4_OUTPUT_4:
            self->preset_4_output_4 = (bool*) data; break;

        case INVERTER_1:
            self->inverter_1 = (bool*) data; break;
        case INVERTER_2:
            self->inverter_2 = (bool*) data; break;
        case INVERTER_3:
            self->inverter_3 = (bool*) data; break;
        case INVERTER_4:
            self->inverter_4 = (bool*) data; break;
    }
}

static void activate(LV2_Handle instance) {}


static unsigned int get_current_preset(Tetr4Switch* self) {
    unsigned int preset_mask = 0b0000
      | *self->preset_selector_1 << 0
      | *self->preset_selector_2 << 1
      | *self->preset_selector_3 << 2
      | *self->preset_selector_4 << 3;
    
    unsigned int index_current_preset = log2(preset_mask & -preset_mask);

    const unsigned int presets[4] = {
        *self->preset_1_output_1 << 0 | *self->preset_1_output_2 << 1 | *self->preset_1_output_3 << 2 | *self->preset_1_output_4 << 3,
        *self->preset_2_output_1 << 0 | *self->preset_2_output_2 << 1 | *self->preset_2_output_3 << 2 | *self->preset_2_output_4 << 3,
        *self->preset_3_output_1 << 0 | *self->preset_3_output_2 << 1 | *self->preset_3_output_3 << 2 | *self->preset_3_output_4 << 3,
        *self->preset_4_output_1 << 0 | *self->preset_4_output_2 << 1 | *self->preset_4_output_3 << 2 | *self->preset_4_output_4 << 3,
    };

    return presets[index_current_preset];
}

static unsigned int get_current_inverter(Tetr4Switch* self) {
    return 0b0000
      | *self->inverter_1 << 0
      | *self->inverter_2 << 1
      | *self->inverter_3 << 2
      | *self->inverter_4 << 3;
}

static void run(LV2_Handle instance, uint32_t n_samples) {
    const unsigned int MAX_TENSION = 10; // volts

    Tetr4Switch* self = (Tetr4Switch*) instance;
    
    unsigned int output = get_current_preset(self) ^ get_current_inverter(self);

    const unsigned int output_cv_1 = output >> 0 * MAX_TENSION;
    const unsigned int output_cv_2 = output >> 1 * MAX_TENSION;
    const unsigned int output_cv_3 = output >> 2 * MAX_TENSION;
    const unsigned int output_cv_4 = output >> 3 * MAX_TENSION;

    for (uint32_t i = 0; i < n_samples; i++) {
        self->output_cv_1[i] = output_cv_1 * MAX_TENSION;
        self->output_cv_2[i] = output_cv_2 * MAX_TENSION;
        self->output_cv_3[i] = output_cv_3 * MAX_TENSION;
        self->output_cv_4[i] = output_cv_4 * MAX_TENSION;
    }
}

static void deactivate(LV2_Handle instance) {}

static void cleanup(LV2_Handle instance) {
    free(instance);
}

static const void* extension_data(const char* uri) {
    return NULL;
}

static const LV2_Descriptor descriptor = {
    PLUGIN_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return index == 0
         ? &descriptor
         : NULL;
}
