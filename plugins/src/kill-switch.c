/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Kill Switch: a rhythmic signal-cut effect, like the momentary kill switch some guitars have. While the plugin is on, a
 * looping step pattern chops the signal; bypassed, it is clean.
 *
 * Standalone: it shares no code with the other plugins (it only needs LV2's core header), so its macros cannot clash
 * with theirs.
 *
 * Patterns: one character per step, `steps_per_beat` steps per beat (before the Speed multiplier). A step has one of
 * three levels, and the gain of each depends on Depth: '0' = cut (the signal is lowered by Depth: silence at 100%),
 * '1' = half (halfway between a cut and the whole signal), '2' = pass (the signal as it is). Every pattern starts
 * with its first step the moment the plugin is switched on, as if a button had been pressed then. The gate can only
 * lower the level, so there is no output limiter.
 *
 * The ten built-in patterns are in PATTERNS; the eleventh, "Custom", is the one the musician writes on the settings
 * page: its length, its steps per beat and its steps are control ports (saved with the pedalboard and the presets).
 *
 * modgui/kill-switch.js repeats the names and the steps of PATTERNS (to draw them): keep the two in step, and the
 * scale points of the `pattern` port in kill-switch.ttl too.
 *
 * The display widget (`pattern`, for devices with the aLabs HMI widgets extension) is in hmi/kill-switch-widgets.c: it draws
 * the pattern, the step that plays and the tempo. It is optional: without the extension the plugin does not draw anything.
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#include "hmi/kill-switch-widgets.h"

#define KILL_SWITCH_URI "http://srmourasilva.github.io/plugins/kill-switch"

#define CUSTOM_MAX_STEPS 16
_Static_assert(CUSTOM_MAX_STEPS == KILL_SWITCH_WIDGET_MAX_STEPS, "the widget keeps as many steps as the custom pattern has");
#define CUSTOM_MAX_STEPS_PER_BEAT 8
#define LEVEL_COUNT 3

/** Port indices: the order of the ports in kill-switch.ttl. New ports go at the end, after `ENABLED` */
typedef enum {
    AUDIO_IN = 0,
    AUDIO_OUT,
    PATTERN,
    BPM,
    SPEED,
    DEPTH,
    EDGE,
    /** The custom pattern: how many steps it has (1..CUSTOM_MAX_STEPS) and how many of them make a beat */
    CUSTOM_LENGTH,
    CUSTOM_STEPS_PER_BEAT,
    /** CUSTOM_MAX_STEPS ports, one per step, in a row: the level (0..2) of each */
    CUSTOM_STEP_FIRST,
    /** `lv2:enabled`, the bypass of the host: 1 = the effect is on, 0 = bypassed (clean). Always the last port */
    ENABLED = CUSTOM_STEP_FIRST + CUSTOM_MAX_STEPS,
    PORT_COUNT
} PortIndex;

typedef struct {
    const char* name;
    int         steps_per_beat;
    const char* steps;
} Pattern;

static const Pattern PATTERNS[] = {
    // Generic grooves
    { "Chop 8ths",            2, "02" },
    { "Chop 16ths",           4, "02" },
    { "Triplet chop",         3, "02" },
    { "Machine gun",          8, "02" },
    { "Syncopated 3-3-2",     2, "02202202" },
    { "Gallop",               4, "0220" },
    // Inspired by guitarists famous for the kill switch: the rhythms are an approximation, not a transcription
    { "Morello solo chop",    4, "0202022002022022" },
    { "Morello toggle solo",  4, "0222020200220202" },
    { "Greenwood chunk",      2, "02220222" },
    { "Buckethead burst",     8, "0202020222222222" },
};

#define BUILT_IN_COUNT ((int) (sizeof(PATTERNS) / sizeof(PATTERNS[0])))
/** The value of the `pattern` port that plays the pattern of the custom ports */
#define CUSTOM_PATTERN BUILT_IN_COUNT
#define PATTERN_COUNT (BUILT_IN_COUNT + 1)

static const float SPEED_MULTIPLIER[] = { 0.5f, 1.0f, 2.0f, 4.0f };
#define SPEED_COUNT ((int) (sizeof(SPEED_MULTIPLIER) / sizeof(SPEED_MULTIPLIER[0])))

#define BPM_MIN 40.0f
#define BPM_MAX 240.0f
#define EDGE_MIN_MS 0.1f
#define EDGE_MAX_MS 20.0f
/** Coming back to the dry signal when the effect is switched off: a fixed time, whatever Edge says */
#define BYPASS_RAMP_SECONDS 0.010f

typedef struct {
    // Ports
    const float* in;
    float*       out;
    const float* pattern;
    const float* bpm;
    const float* speed;
    const float* depth;
    const float* edge;
    const float* custom_length;
    const float* custom_steps_per_beat;
    const float* custom_step[CUSTOM_MAX_STEPS];
    const float* enabled;

    float  sample_rate;
    /** Samples since the current step began (a double, so a long loop does not drift) */
    double position;
    int    step;
    /** Current gain: 1 = the signal passes, 1 - Depth = cut */
    float  gain;
    bool   was_bypassed;

    /** The display widgets: what they show and which are on a display (hmi/kill-switch-widgets.c) */
    KillSwitchWidgets widgets;
} KillSwitch;

static float clampf(float value, float min, float max) {
    return value < min ? min : (value > max ? max : value);
}

/** @return the index in 0..count-1 of the nearest integer of a (possibly not yet connected, or automated) port value */
static int choice(const float* port, int count) {
    int value = (int) (port ? *port + 0.5f : 0.0f);
    return value < 0 ? 0 : (value > count - 1 ? count - 1 : value);
}

static void reset(KillSwitch* self) {
    self->position = 0.0;
    self->step = 0;
    // Starts at unity: bypassed from the first sample means no fade
    self->gain = 1.0f;
    self->was_bypassed = true;
}

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
    KillSwitch* self = (KillSwitch*) calloc(1, sizeof(KillSwitch));

    if (self == NULL) {
        return NULL;
    }

    self->sample_rate = (float) rate;
    reset(self);
    kill_switch_widgets_init(&self->widgets, features);

    return (LV2_Handle) self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    KillSwitch* self = (KillSwitch*) instance;

    switch ((PortIndex) port) {
        case AUDIO_IN: self->in      = (const float*) data; break;
        case AUDIO_OUT: self->out    = (float*) data;       break;
        case PATTERN:  self->pattern = (const float*) data; break;
        case BPM:      self->bpm     = (const float*) data; break;
        case SPEED:    self->speed   = (const float*) data; break;
        case DEPTH:    self->depth   = (const float*) data; break;
        case EDGE:     self->edge    = (const float*) data; break;
        case CUSTOM_LENGTH:          self->custom_length          = (const float*) data; break;
        case CUSTOM_STEPS_PER_BEAT:  self->custom_steps_per_beat  = (const float*) data; break;
        case ENABLED:  self->enabled = (const float*) data; break;
        default:
            if (port >= CUSTOM_STEP_FIRST && port < CUSTOM_STEP_FIRST + CUSTOM_MAX_STEPS) {
                self->custom_step[port - CUSTOM_STEP_FIRST] = (const float*) data;
            }
            break;
    }
}

static void activate(LV2_Handle instance) {
    reset((KillSwitch*) instance);
}

// ---------------------------------------------------------------------------------------------------------------------
// The display widgets (hmi/kill-switch-widgets.c draw them; here only what they show is filled in)

static void update_view(KillSwitch* self, bool bypassed, int pattern_index, const char* steps, int count, float steps_per_second_factor, float bpm) {
    KillSwitchView* view = &self->widgets.view;

    view->bypassed = bypassed;
    snprintf(view->name, sizeof(view->name), "%s", pattern_index == CUSTOM_PATTERN ? "Custom" : PATTERNS[pattern_index].name);
    memcpy(view->steps, steps, (size_t) count);
    view->steps[count] = '\0';
    view->count = count;
    view->step = self->step;
    view->bpm = bpm;
    view->speed = choice(self->speed, SPEED_COUNT);
    view->depth = (int) (clampf(self->depth ? *self->depth : 100.0f, 0.0f, 100.0f) + 0.5f);
    view->fast = (double) bpm * (double) steps_per_second_factor / 60.0 > KILL_SWITCH_WIDGET_MAX_FOLLOW_STEPS_PER_SECOND;
}

static void widget_shown(LV2_Handle instance, const char* symbol, ALABS_HMI_Widget widget, const ALABS_HMI_WidgetArea* area) {
    widgets_shown(&((KillSwitch*) instance)->widgets.host, symbol, widget, area);
}

static void widget_hidden(LV2_Handle instance, ALABS_HMI_Widget widget) {
    widgets_hidden(&((KillSwitch*) instance)->widgets.host, widget);
}

static void run(LV2_Handle instance, uint32_t frames) {
    KillSwitch* self = (KillSwitch*) instance;

    // Nothing to do (or to write to) until the host has connected the audio ports
    if (self->in == NULL || self->out == NULL) {
        return;
    }

    const bool  bypassed  = self->enabled != NULL && *self->enabled < 0.5f;
    const int   pattern_index = choice(self->pattern, PATTERN_COUNT);
    // The steps of the pattern, one character per step: '0'..'2' (a built-in one points into PATTERNS)
    char        custom_steps[CUSTOM_MAX_STEPS + 1];
    const char* steps;
    int         steps_count;
    int         steps_per_beat;

    if (pattern_index == CUSTOM_PATTERN) {
        steps_count = (int) clampf(self->custom_length ? *self->custom_length + 0.5f : 8.0f, 1.0f, (float) CUSTOM_MAX_STEPS);
        steps_per_beat = (int) clampf(self->custom_steps_per_beat ? *self->custom_steps_per_beat + 0.5f : 2.0f, 1.0f, (float) CUSTOM_MAX_STEPS_PER_BEAT);

        for (int i = 0; i < steps_count; ++i) {
            custom_steps[i] = (char) ('0' + choice(self->custom_step[i], LEVEL_COUNT));
        }
        custom_steps[steps_count] = '\0';
        steps = custom_steps;
    } else {
        steps = PATTERNS[pattern_index].steps;
        steps_count = (int) strlen(steps);
        steps_per_beat = PATTERNS[pattern_index].steps_per_beat;
    }

    const float multiplier = SPEED_MULTIPLIER[choice(self->speed, SPEED_COUNT)];
    const float bpm        = clampf(self->bpm ? *self->bpm : 120.0f, BPM_MIN, BPM_MAX);
    const float floor_gain = 1.0f - clampf(self->depth ? *self->depth * 0.01f : 1.0f, 0.0f, 1.0f);
    const float edge_ms    = clampf(self->edge ? *self->edge : 1.5f, EDGE_MIN_MS, EDGE_MAX_MS);

    // The gain of each level: a cut is as deep as Depth says, the middle level halfway between a cut and the whole signal
    const float level_gain[LEVEL_COUNT] = { floor_gain, floor_gain + (1.0f - floor_gain) * 0.5f, 1.0f };

    double step_length = 60.0 * (double) self->sample_rate / ((double) bpm * (double) steps_per_beat * (double) multiplier);
    if (step_length < 2.0) {
        step_length = 2.0;
    }

    // Slew per sample: the Edge time while chopping, a fixed 10 ms back to the dry signal on bypass
    const float slew_chop   = 1.0f / (edge_ms * 0.001f * self->sample_rate);
    const float slew_bypass = 1.0f / (BYPASS_RAMP_SECONDS * self->sample_rate);

    if (!bypassed && self->was_bypassed) {
        // Switched on: the pattern starts now, with its first step
        self->position = 0.0;
        self->step = 0;
    }
    self->was_bypassed = bypassed;

    // The pattern was changed to a shorter one
    if (self->step >= steps_count) {
        self->step = 0;
    }

    for (uint32_t i = 0; i < frames; ++i) {
        float target = 1.0f;
        float slew = slew_bypass;

        if (!bypassed) {
            target = level_gain[steps[self->step] - '0'];
            slew = slew_chop;
        }

        if (self->gain < target) {
            self->gain += slew;
            if (self->gain > target) self->gain = target;
        } else if (self->gain > target) {
            self->gain -= slew;
            if (self->gain < target) self->gain = target;
        }

        self->out[i] = self->in[i] * self->gain;

        // The clock only runs while the effect is on
        if (!bypassed) {
            self->position += 1.0;

            if (self->position >= step_length) {
                self->position -= step_length;

                if (++self->step >= steps_count) {
                    self->step = 0;
                }
            }
        }
    }

    update_view(self, bypassed, pattern_index, steps, steps_count, steps_per_beat * multiplier, bpm);
    kill_switch_widgets_run(&self->widgets, frames, (double) self->sample_rate);
}

static void deactivate(LV2_Handle instance) {}

static void cleanup(LV2_Handle instance) {
    free(instance);
}

static const void* extension_data(const char* uri) {
    static const ALABS_HMI_WidgetNotification widget_notification = {
        sizeof(ALABS_HMI_WidgetNotification),
        widget_shown,
        widget_hidden,
    };

    if (strcmp(uri, ALABS_HMI__WidgetNotification) == 0) {
        return &widget_notification;
    }

    return NULL;
}

static const LV2_Descriptor descriptor = {
    KILL_SWITCH_URI,
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
