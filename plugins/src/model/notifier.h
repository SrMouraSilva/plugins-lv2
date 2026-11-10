/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NOTIFIER_H
#define	NOTIFIER_H

#include "../lv2/extension/lv2-hmi.h"


typedef struct {
    /** LV2 input */
    float* input;

    LV2_HMI_Addressing hmi_addressing;
} Notifier;

#endif