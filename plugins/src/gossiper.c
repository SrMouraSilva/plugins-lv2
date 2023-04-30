#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#include "model/gossiper.h"
#include "lv2/gossiper/lv2-gossiper.h"
#include "lv2/gossiper/lv2-hmi-gossiper.h"

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
    Gossiper* self = Gossiper_instantiate();

    self->lv2 = LV2_Gossiper_instantiate();

    const char* missing = self->lv2->initialize(self->lv2, features);

    if (missing) {
        //cleanup(self);
        //free(self->lv2);
        free(self);
        return NULL;
    }

    // Atom_instantiate(self);

    return (LV2_Handle) self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    Gossiper* self = (Gossiper*) instance;

    switch ((PortIndex) port) {
        case OUTPUT_CV_1:
            self->switches[0].output = (float*) data; break;
        case OUTPUT_CV_2:
            self->switches[1].output = (float*) data; break;
        case OUTPUT_CV_3:
            self->switches[2].output = (float*) data; break;
        case OUTPUT_CV_4:
            self->switches[3].output = (float*) data; break;
        case OUTPUT_CV_5:
            self->switches[4].output = (float*) data; break;
        case OUTPUT_CV_6:
            self->switches[5].output = (float*) data; break;
        case OUTPUT_CV_7:
            self->switches[6].output = (float*) data; break;
        
        case FOOTSWITCH_1:
            self->switches[0].input = (float*) data; break;
        case FOOTSWITCH_2:
            self->switches[1].input = (float*) data; break;
        case FOOTSWITCH_3:
            self->switches[2].input = (float*) data; break;
        case FOOTSWITCH_4:
            self->switches[3].input = (float*) data; break;
        case FOOTSWITCH_5:
            self->switches[4].input = (float*) data; break;
        case FOOTSWITCH_6:
            self->switches[5].input = (float*) data; break;
        case FOOTSWITCH_7:
            self->switches[6].input = (float*) data; break;

        case POTENTIOMETER_1:
            self->potentiometers[0].input = (float*) data; break;
        case MINIMUM_1:
            self->potentiometers[0].minimum = (float*) data; break;
        case MAXIMUM_1:
            self->potentiometers[0].maximum = (float*) data; break;
        case OUTPUT_POTENTIOMETER_CV_1:
            self->potentiometers[0].output = (float*) data; break;

        case POTENTIOMETER_2:
            self->potentiometers[1].input = (float*) data; break;
        case MINIMUM_2:
            self->potentiometers[1].minimum = (float*) data; break;
        case MAXIMUM_2:
            self->potentiometers[1].maximum = (float*) data; break;
        case OUTPUT_POTENTIOMETER_CV_2:
            self->potentiometers[1].output = (float*) data; break;

        case POTENTIOMETER_3:
            self->potentiometers[2].input = (float*) data; break;
        case MINIMUM_3:
            self->potentiometers[2].minimum = (float*) data; break;
        case MAXIMUM_3:
            self->potentiometers[2].maximum = (float*) data; break;
        case OUTPUT_POTENTIOMETER_CV_3:
            self->potentiometers[2].output = (float*) data; break;

        case POTENTIOMETER_4:
            self->potentiometers[3].input = (float*) data; break;
        case MINIMUM_4:
            self->potentiometers[3].minimum = (float*) data; break;
        case MAXIMUM_4:
            self->potentiometers[3].maximum = (float*) data; break;
        case OUTPUT_POTENTIOMETER_CV_4:
            self->potentiometers[3].output = (float*) data; break;

        case ASSIGN_TO_NOTIFY_1:
        case ASSIGN_TO_NOTIFY_2:
        case ASSIGN_TO_NOTIFY_3:
        case ASSIGN_TO_NOTIFY_4:
            break;

        // case EVENTS_IN:
        //     self->lv2->atom.events_in = (const LV2_Atom_Sequence*) data; break;
    }
}

static void activate(LV2_Handle instance) {}


static void run(LV2_Handle instance, uint32_t n_samples) {
    Gossiper* self = (Gossiper*) instance;

    self->run(self, n_samples);

    LV2_HMI_Gossiper_run(self);
    //Atom_run(self);
}


static void deactivate(LV2_Handle instance) {}

static void cleanup(LV2_Handle instance) {
    Gossiper* self = (Gossiper*) instance;

    free(self->lv2);
    self->lv2 = NULL;

    free(self);
}

static const void* extension_data(const char* uri) {
    if (LV2_HMI_Gossiper_is_extension_data_appliable(uri)) {
        return LV2_HMI_Gossiper_extension_data();
    }
    
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
