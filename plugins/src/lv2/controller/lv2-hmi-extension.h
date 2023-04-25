#ifndef LV2_HMI_EXTENSION_H
#define	LV2_HMI_EXTENSION_H

#include "../../model/controller.h"



extern bool LV2_HMI_is_extension_data_appliable(const char* uri);
extern const void* LV2_HMI_extension_data();

extern void LV2_HMI_addressed(LV2_Handle handle, uint32_t index, LV2_HMI_Addressing addressing, const LV2_HMI_AddressingInfo* info);
extern void LV2_HMI_unaddressed(LV2_Handle handle, uint32_t index);

extern void LV2_HMI_run(Controller* self);

#endif