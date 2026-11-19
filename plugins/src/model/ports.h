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
 *   Dip switches      TOTAL_PRESETS * TOTAL_OUTPUTS ports (preset by preset)
 *   Events in/out     2 ports
 *   In the cycle      TOTAL_PRESETS ports, Hept7 Switch only (added after the events: ports are never renumbered)
 *   Active preset     1 output port, Hept7 Switch only
 *   MIDI              TOTAL_OUTPUTS CC numbers, the channel, "changes only" and the MIDI output (added last, for the
 *                     same reason)
 *   Next, Previous    2 buttons, Hept7 Switch only (after the MIDI output). "Current preset" is a button there too
 *
 * Tetr4 Switch (4x4):  outputs 0-3, selectors 4-7, current preset 8, notify 9-12, dips 13-28, events 29-30,
 *                      MIDI CC 31-34, MIDI channel 35, changes only 36, MIDI out 37
 * Hept7 Switch (7 presets x 8 outputs):  outputs 0-7, selectors 8-14, current preset 15, notify 16-19, dips 20-75,
 *                                        events 76-77, in the cycle 78-84, active preset 85,
 *                                        MIDI CC 86-93, MIDI channel 94, changes only 95, MIDI out 96,
 *                                        next preset 97, previous preset 98
 */
#define PORT_OUTPUT_CV_FIRST          0
#define PORT_PRESET_SELECTOR_FIRST    (PORT_OUTPUT_CV_FIRST + TOTAL_OUTPUTS)
#define PORT_PRESET_SELECT            (PORT_PRESET_SELECTOR_FIRST + TOTAL_PRESETS)
#define PORT_ASSIGN_TO_NOTIFY_FIRST   (PORT_PRESET_SELECT + 1)
#define PORT_PRESET_OUTPUT_FIRST      (PORT_ASSIGN_TO_NOTIFY_FIRST + TOTAL_CONTROLLER_NOTIFIERS)
#define PORT_EVENTS_IN                (PORT_PRESET_OUTPUT_FIRST + TOTAL_PRESETS * TOTAL_OUTPUTS)
#define PORT_EVENTS_OUT               (PORT_EVENTS_IN + 1)
#ifdef HEPT7_SWITCH
/** Whether a preset is one of those "Current preset" goes through. Hept7 Switch only: a plugin without these ports
 *  (Tetr4 Switch) has every preset in the cycle */
#define PORT_PRESET_IN_CYCLE_FIRST    (PORT_EVENTS_OUT + 1)
/** Output: the active preset (0-based), for the interface. Hept7 Switch only */
#define PORT_ACTIVE_PRESET            (PORT_PRESET_IN_CYCLE_FIRST + TOTAL_PRESETS)
#define PORT_MIDI_CC_FIRST            (PORT_ACTIVE_PRESET + 1)
#else
#define PORT_MIDI_CC_FIRST            (PORT_EVENTS_OUT + 1)
#endif
/** Control change number sent for each CV output (one port per output) */
#define PORT_MIDI_CHANNEL             (PORT_MIDI_CC_FIRST + TOTAL_OUTPUTS)
/** Only send a CC when its value differs from the last one sent */
#define PORT_MIDI_CHANGES_ONLY        (PORT_MIDI_CHANNEL + 1)
/** Output: the control changes, one per CV output, whenever the preset changes */
#define PORT_MIDI_OUT                 (PORT_MIDI_CHANGES_ONLY + 1)
#ifdef HEPT7_SWITCH
/** Buttons: go to the next / the previous preset in the cycle. Hept7 Switch only */
#define PORT_NEXT_PRESET              (PORT_MIDI_OUT + 1)
#define PORT_PREVIOUS_PRESET          (PORT_NEXT_PRESET + 1)
#endif

/** Index of the dip switch that sets CV output @p output (0-based) while preset @p preset (0-based) is active */
#define PORT_PRESET_OUTPUT(preset, output) (PORT_PRESET_OUTPUT_FIRST + (preset) * TOTAL_OUTPUTS + (output))

#ifndef HEPT7_SWITCH
// The Tetr4 Switch TTL numbers its ports by hand: guard against the layout drifting from it
_Static_assert(PORT_PRESET_SELECTOR_FIRST == 4, "tetr4-switch.ttl: preset selectors start at port 4");
_Static_assert(PORT_PRESET_SELECT == 8, "tetr4-switch.ttl: current preset is port 8");
_Static_assert(PORT_ASSIGN_TO_NOTIFY_FIRST == 9, "tetr4-switch.ttl: assign to notify starts at port 9");
_Static_assert(PORT_PRESET_OUTPUT_FIRST == 13, "tetr4-switch.ttl: dip switches start at port 13");
_Static_assert(PORT_EVENTS_IN == 29, "tetr4-switch.ttl: events in is port 29");
_Static_assert(PORT_EVENTS_OUT == 30, "tetr4-switch.ttl: events out is port 30");
_Static_assert(PORT_MIDI_CC_FIRST == 31, "tetr4-switch.ttl: MIDI CC numbers start at port 31");
_Static_assert(PORT_MIDI_CHANNEL == 35, "tetr4-switch.ttl: MIDI channel is port 35");
_Static_assert(PORT_MIDI_CHANGES_ONLY == 36, "tetr4-switch.ttl: MIDI changes only is port 36");
_Static_assert(PORT_MIDI_OUT == 37, "tetr4-switch.ttl: MIDI out is port 37");
#else
// The Hept7 Switch TTL is long and generated: guard the part added last, which is easy to get wrong
_Static_assert(PORT_ACTIVE_PRESET == 85, "hept7-switch.ttl: active preset is port 85");
_Static_assert(PORT_MIDI_CC_FIRST == 86, "hept7-switch.ttl: MIDI CC numbers start at port 86");
_Static_assert(PORT_MIDI_CHANNEL == 94, "hept7-switch.ttl: MIDI channel is port 94");
_Static_assert(PORT_MIDI_CHANGES_ONLY == 95, "hept7-switch.ttl: MIDI changes only is port 95");
_Static_assert(PORT_MIDI_OUT == 96, "hept7-switch.ttl: MIDI out is port 96");
_Static_assert(PORT_NEXT_PRESET == 97, "hept7-switch.ttl: next preset is port 97");
_Static_assert(PORT_PREVIOUS_PRESET == 98, "hept7-switch.ttl: previous preset is port 98");
#endif

#endif
