#ifndef LV2_HMI_GOSSIPER_H
#define	LV2_HMI_GOSSIPER_H

#include "../../model/gossiper.h"


extern bool LV2_HMI_Gossiper_is_extension_data_appliable(const char* uri);
extern const void* LV2_HMI_Gossiper_extension_data();

extern void LV2_HMI_Gossiper_run(Gossiper* self);

#endif