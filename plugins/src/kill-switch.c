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
 * Patterns: '0' = cut, '1' = pass, one character per step, `steps_per_beat` steps per beat (before the Speed
 * multiplier). Every pattern starts with its first character the moment the plugin is switched on, as if a button had
 * been pressed then. The gate can only lower the level, so there is no output limiter.
 *
 * modgui/kill-switch.js repeats the names and the steps of PATTERNS (to draw them): keep the two in step, and the
 * scale points of the `pattern` port in kill-switch.ttl too.
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define KILL_SWITCH_URI "http://srmourasilva.github.io/plugins/kill-switch"

/** Port indices: the order of the ports in kill-switch.ttl. New ports go at the end, after `ENABLED` */
typedef enum {
    AUDIO_IN = 0,
    AUDIO_OUT,
    PATTERN,
    BPM,
    SPEED,
    DEPTH,
    EDGE,
    /** `lv2:enabled`, the bypass of the host: 1 = the effect is on, 0 = bypassed (clean) */
    ENABLED,
    PORT_COUNT
} PortIndex;

typedef struct {
    const char* name;
    int         steps_per_beat;
    const char* steps;
} Pattern;

static const Pattern PATTERNS[] = {
    // Generic grooves
    { "Chop 8ths",            2, "01" },
    { "Chop 16ths",           4, "01" },
    { "Triplet chop",         3, "01" },
    { "Machine gun",          8, "01" },
    { "Syncopated 3-3-2",     2, "01101101" },
    { "Gallop",               4, "0110" },
    // Inspired by guitarists famous for the kill switch: the rhythms are an approximation, not a transcription
    { "Morello solo chop",    4, "0101011001011011" },
    { "Morello toggle solo",  4, "0111010100110101" },
    { "Greenwood chunk",      2, "01110111" },
    { "Buckethead burst",     8, "0101010111111111" },
};

#define PATTERN_COUNT ((int) (sizeof(PATTERNS) / sizeof(PATTERNS[0])))

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
    const float* enabled;

    float  sample_rate;
    /** Samples since the current step began (a double, so a long loop does not drift) */
    double position;
    int    step;
    /** Current gain: 1 = the signal passes, 1 - Depth = cut */
    float  gain;
    bool   was_bypassed;
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
        case ENABLED:  self->enabled = (const float*) data; break;
        default: break;
    }
}

static void activate(LV2_Handle instance) {
    reset((KillSwitch*) instance);
}

static void run(LV2_Handle instance, uint32_t frames) {
    KillSwitch* self = (KillSwitch*) instance;

    // Nothing to do (or to write to) until the host has connected the audio ports
    if (self->in == NULL || self->out == NULL) {
        return;
    }

    const bool  bypassed  = self->enabled != NULL && *self->enabled < 0.5f;
    const Pattern* pattern = &PATTERNS[choice(self->pattern, PATTERN_COUNT)];
    const int   steps_count = (int) strlen(pattern->steps);
    const float multiplier = SPEED_MULTIPLIER[choice(self->speed, SPEED_COUNT)];
    const float bpm        = clampf(self->bpm ? *self->bpm : 120.0f, BPM_MIN, BPM_MAX);
    const float floor_gain = 1.0f - clampf(self->depth ? *self->depth * 0.01f : 1.0f, 0.0f, 1.0f);
    const float edge_ms    = clampf(self->edge ? *self->edge : 1.5f, EDGE_MIN_MS, EDGE_MAX_MS);

    double step_length = 60.0 * (double) self->sample_rate / ((double) bpm * (double) pattern->steps_per_beat * (double) multiplier);
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
            target = pattern->steps[self->step] == '1' ? 1.0f : floor_gain;
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
}

static void deactivate(LV2_Handle instance) {}

static void cleanup(LV2_Handle instance) {
    free(instance);
}

static const void* extension_data(const char* uri) {
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
