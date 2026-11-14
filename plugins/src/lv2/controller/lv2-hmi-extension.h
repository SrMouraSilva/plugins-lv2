/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LV2_HMI_EXTENSION_H
#define	LV2_HMI_EXTENSION_H

#include <lv2/worker/worker.h>

#include "../../model/controller.h"


extern bool LV2_HMI_is_extension_data_appliable(const char* uri);
extern const void* LV2_HMI_extension_data(void);

/** After Controller_run(): on a change of preset, update the device (from the worker when the host has one) */
extern void LV2_HMI_run(Controller* self);

/** LV2_Worker_Interface: the worker thread sends to the HMI what LV2_HMI_run() scheduled */
extern LV2_Worker_Status LV2_HMI_work(LV2_Handle instance,
                                      LV2_Worker_Respond_Function respond,
                                      LV2_Worker_Respond_Handle handle,
                                      uint32_t size,
                                      const void* data);
extern LV2_Worker_Status LV2_HMI_work_response(LV2_Handle instance, uint32_t size, const void* body);

#endif
