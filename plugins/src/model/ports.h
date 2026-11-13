/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PORTS_H
#define	PORTS_H

#include "../config.h"

/*
 * Port layout, shared by the TTL files and the code. Every group is a contiguous
 * range, so a port is found by "first index + offset" instead of one case per port:
 *
 *   CV outputs        TOTAL_OUTPUTS ports
 *   Preset selectors  TOTAL_PRESETS ports (footswitch selectors)
 *   Current preset    1 port
 *   Assign to notify  TOTAL_CONTROLLER_NOTIFIERS ports
 *   Dip switches      TOTAL_PRESETS * TOTAL_DIPS ports (preset by preset)
 *   Events in/out     2 ports
 *
 * Tetr4 Switch (4x4):  outputs 0-3, selectors 4-7, current preset 8, notify 9-12, dips 13-28, events 29-30
 * Hept7 Switch (7x7, 8 dips per preset): outputs 0-6, selectors 7-13, current preset 14, notify 15-18, dips 19-74, events 75-76
 */
#define PORT_OUTPUT_CV_FIRST          0
#define PORT_PRESET_SELECTOR_FIRST    (PORT_OUTPUT_CV_FIRST + TOTAL_OUTPUTS)
#define PORT_PRESET_SELECT            (PORT_PRESET_SELECTOR_FIRST + TOTAL_PRESETS)
#define PORT_ASSIGN_TO_NOTIFY_FIRST   (PORT_PRESET_SELECT + 1)
#define PORT_PRESET_OUTPUT_FIRST      (PORT_ASSIGN_TO_NOTIFY_FIRST + TOTAL_CONTROLLER_NOTIFIERS)
#define PORT_EVENTS_IN                (PORT_PRESET_OUTPUT_FIRST + TOTAL_PRESETS * TOTAL_DIPS)
#define PORT_EVENTS_OUT               (PORT_EVENTS_IN + 1)

/**
 * Index of the dip switch @p dip (0-based) of preset @p preset (0-based): the first TOTAL_OUTPUTS ones set the
 * CV outputs while the preset is active; the next one (if HAS_POPUP_SWITCH) turns its HMI pop-up on/off
 */
#define PORT_PRESET_OUTPUT(preset, dip) (PORT_PRESET_OUTPUT_FIRST + (preset) * TOTAL_DIPS + (dip))

#ifndef HEPT7_SWITCH
// The Tetr4 Switch TTL numbers its ports by hand: guard against the layout drifting from it
_Static_assert(PORT_PRESET_SELECTOR_FIRST == 4, "tetr4-switch.ttl: preset selectors start at port 4");
_Static_assert(PORT_PRESET_SELECT == 8, "tetr4-switch.ttl: current preset is port 8");
_Static_assert(PORT_ASSIGN_TO_NOTIFY_FIRST == 9, "tetr4-switch.ttl: assign to notify starts at port 9");
_Static_assert(PORT_PRESET_OUTPUT_FIRST == 13, "tetr4-switch.ttl: dip switches start at port 13");
_Static_assert(PORT_EVENTS_IN == 29, "tetr4-switch.ttl: events in is port 29");
_Static_assert(PORT_EVENTS_OUT == 30, "tetr4-switch.ttl: events out is port 30");
#endif

#endif
