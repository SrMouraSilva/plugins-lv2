/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Half Mouco: the stereo sibling of Kill Switch. A looping step pattern chops the signal, and the left and the right
 * side each have a row of their own, so the sound changes with the side (a ping-pong, an ear that is chopped while the
 * other one is not...). A second family, the "endings", plays once when the plugin is switched on and finishes in
 * silence: effects for the end of a song (a bouncing ball, a helicopter that lands...), that the musician chooses but
 * does not draw.
 *
 * Standalone, like kill-switch.c: it only needs LV2's core header.
 *
 * Steps: one character per step, `steps_per_beat` steps per beat (before the Speed multiplier). A step has one of three
 * levels: '0' = cut (the signal is lowered by Depth: silence at 100%), '1' = half (halfway between a cut and the whole
 * signal), '2' = pass. Every pattern starts with its first step the moment the plugin is switched on.
 *
 * Endings: the gate (0 = cut .. 1 = pass) of each side is a function of the time since the plugin was switched on
 * (see ending_gate), which lasts `length` beats at the tempo; after it, everything is cut.
 *
 * modgui/half-mouco.js repeats the names, the steps and the endings (to draw them): keep them in step, and the scale
 * points of `pattern` and `ending` in half-mouco.ttl too.
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define HALF_MOUCO_URI "http://srmourasilva.github.io/plugins/half-mouco"

#define CUSTOM_MAX_STEPS 16
#define CUSTOM_MAX_STEPS_PER_BEAT 8
#define LEVEL_COUNT 3

/** Port indices: the order of the ports in half-mouco.ttl. */
typedef enum {
    AUDIO_IN_L = 0,
    AUDIO_IN_R,
    AUDIO_OUT_L,
    AUDIO_OUT_R,
    /** 0 = the patterns (they loop), 1 = the endings (they play once) */
    MODE,
    PATTERN,
    ENDING,
    BPM,
    SPEED,
    /** How long an ending lasts, in beats */
    LENGTH,
    DEPTH,
    EDGE,
    /** 1 = the left input feeds both sides */
    MONO,
    CUSTOM_LENGTH,
    CUSTOM_STEPS_PER_BEAT,
    /** CUSTOM_MAX_STEPS ports for the left side, then CUSTOM_MAX_STEPS for the right side: the level (0..2) of each step */
    CUSTOM_LEFT_FIRST,
    CUSTOM_RIGHT_FIRST = CUSTOM_LEFT_FIRST + CUSTOM_MAX_STEPS,
    /** `lv2:enabled`, the bypass of the host: 1 = the effect is on, 0 = bypassed (clean). Always the last port */
    ENABLED = CUSTOM_RIGHT_FIRST + CUSTOM_MAX_STEPS,
    PORT_COUNT
} PortIndex;

typedef struct {
    const char* name;
    int         steps_per_beat;
    const char* left;
    const char* right;
} Pattern;

static const Pattern PATTERNS[] = {
    { "Ping-pong 8ths",      2, "20",       "02" },
    { "Ping-pong 16ths",     4, "20",       "02" },
    { "Triplet pong",        3, "200",      "020" },
    { "Half deaf: left",     2, "02",       "22" },
    { "Half deaf: right",    2, "22",       "02" },
    { "Gallop sides",        4, "2200",     "0022" },
    { "Stagger",             4, "2002",     "0220" },
    { "Seesaw",              2, "2210",     "0122" },
    { "Syncopated sides",    2, "20020020", "02002002" },
};

#define BUILT_IN_COUNT ((int) (sizeof(PATTERNS) / sizeof(PATTERNS[0])))
/** The value of the `pattern` port that plays the pattern of the custom ports */
#define CUSTOM_PATTERN BUILT_IN_COUNT
#define PATTERN_COUNT (BUILT_IN_COUNT + 1)

typedef enum {
    ENDING_BOUNCING_BALL = 0,
    ENDING_HELICOPTER,
    ENDING_STUTTER_OUT,
    ENDING_SIDE_FADE,
    ENDING_PING_PONG_OUT,
    ENDING_COUNT
} Ending;

static const float SPEED_MULTIPLIER[] = { 0.5f, 1.0f, 2.0f, 4.0f };
#define SPEED_COUNT ((int) (sizeof(SPEED_MULTIPLIER) / sizeof(SPEED_MULTIPLIER[0])))

#define BPM_MIN 40.0f
#define BPM_MAX 240.0f
#define LENGTH_MIN 2.0f
#define LENGTH_MAX 32.0f
#define EDGE_MIN_MS 0.1f
#define EDGE_MAX_MS 20.0f
/** Coming back to the dry signal when the effect is switched off: a fixed time, whatever Edge says */
#define BYPASS_RAMP_SECONDS 0.010f

/** The bouncing ball: every gap is this fraction of the one before, so the bounces pile up at the end of the ending */
#define BOUNCE_RATIO 0.78
/** ... and a bounce is this fraction of its gap long (never longer than BOUNCE_MAX_BEATS). When a gap is shorter than BOUNCE_END_SECONDS the ball has stopped */
#define BOUNCE_OPEN 0.55
#define BOUNCE_MAX_BEATS 0.35
#define BOUNCE_END_SECONDS 0.001
/** The helicopter: the gap between two blades grows by this factor, from a sixteenth note */
#define HELICOPTER_GROWTH 1.12
#define HELICOPTER_OPEN 0.5

typedef struct {
    // Ports
    const float* in_l;
    const float* in_r;
    float*       out_l;
    float*       out_r;
    const float* mode;
    const float* pattern;
    const float* ending;
    const float* bpm;
    const float* speed;
    const float* length;
    const float* depth;
    const float* edge;
    const float* mono;
    const float* custom_length;
    const float* custom_steps_per_beat;
    const float* custom_left[CUSTOM_MAX_STEPS];
    const float* custom_right[CUSTOM_MAX_STEPS];
    const float* enabled;

    float  sample_rate;
    /** Samples since the current step began (a double, so a long loop does not drift) */
    double position;
    int    step;
    /** Seconds since the plugin was switched on, for the endings */
    double elapsed;
    /** The mode and the ending of the last run: a change starts the ending again */
    int    last_mode;
    int    last_ending;
    /** Current gain of each side: 1 = the signal passes, 1 - Depth = cut */
    float  gain_l;
    float  gain_r;
    bool   was_bypassed;
} HalfMouco;

static float clampf(float value, float min, float max) {
    return value < min ? min : (value > max ? max : value);
}

/** @return the index in 0..count-1 of the nearest integer of a (possibly not yet connected, or automated) port value */
static int choice(const float* port, int count) {
    int value = (int) (port ? *port + 0.5f : 0.0f);
    return value < 0 ? 0 : (value > count - 1 ? count - 1 : value);
}

static void reset(HalfMouco* self) {
    self->position = 0.0;
    self->step = 0;
    self->elapsed = 0.0;
    self->last_mode = 0;
    self->last_ending = 0;
    // Starts at unity: bypassed from the first sample means no fade
    self->gain_l = 1.0f;
    self->gain_r = 1.0f;
    self->was_bypassed = true;
}

/*
 * The endings. `t` is the time in seconds since the plugin was switched on, `total` how long the ending lasts and `beat`
 * the length of a beat, both in seconds. Gives the gate of each side: 0 = cut, 1 = pass. After `total` everything is cut.
 * `state` keeps what the ending found out at the last call (the index of the bounce or of the blade), so that a block
 * does not search from the start; it is only a cache: a wrong value is corrected.
 *
 * modgui/half-mouco.js has the same functions (to draw the ending): keep them in step.
 */
typedef struct {
    int index;
} EndingState;

static void ending_gate(Ending ending, double t, double total, double beat, EndingState* state, float* gate_l, float* gate_r) {
    *gate_l = 0.0f;
    *gate_r = 0.0f;

    if (t < 0.0 || t >= total) {
        return;
    }

    const double u = t / total;

    switch (ending) {
        case ENDING_BOUNCING_BALL: {
            // Bounce k starts at total * (1 - r^k): the gaps shrink by r each time and pile up at the end
            int k = state->index;
            if (k < 0) k = 0;
            while (k > 0 && t < total * (1.0 - pow(BOUNCE_RATIO, k))) --k;
            while (t >= total * (1.0 - pow(BOUNCE_RATIO, k + 1))) ++k;
            state->index = k;

            const double start = total * (1.0 - pow(BOUNCE_RATIO, k));
            const double gap = total * pow(BOUNCE_RATIO, k) * (1.0 - BOUNCE_RATIO);
            if (gap < BOUNCE_END_SECONDS) {
                return;
            }
            double open = gap * BOUNCE_OPEN;
            if (open > beat * BOUNCE_MAX_BEATS) open = beat * BOUNCE_MAX_BEATS;
            if (t - start < open) {
                *gate_l = *gate_r = 1.0f;
            }
            break;
        }

        case ENDING_HELICOPTER: {
            // Blade k starts at p0 * (g^k - 1) / (g - 1): the gap between blades grows, and the blades go to one side then the other
            const double p0 = beat * 0.25;
            const double g = HELICOPTER_GROWTH;
            int k = (int) (log(1.0 + t * (g - 1.0) / p0) / log(g));
            if (k < 0) k = 0;
            const double start = p0 * (pow(g, k) - 1.0) / (g - 1.0);
            const double period = p0 * pow(g, k);
            if (t - start < period * HELICOPTER_OPEN) {
                const float level = (float) (1.0 - u);
                if (k % 2 == 0) *gate_l = level; else *gate_r = level;
            }
            break;
        }

        case ENDING_STUTTER_OUT: {
            // Sixteenth notes whose open part shrinks to nothing; the right side is half a period behind the left one
            const double period = beat * 0.25;
            const double duty = 0.9 * (1.0 - u) + 0.02;
            const double phase_l = fmod(t, period) / period;
            const double phase_r = fmod(t + period * 0.5, period) / period;
            if (phase_l < duty) *gate_l = 1.0f;
            if (phase_r < duty) *gate_r = 1.0f;
            break;
        }

        case ENDING_SIDE_FADE: {
            // Eighth notes, half open; the left side fades out first (by 60% of the ending), the right one at the end
            const double period = beat * 0.5;
            if (fmod(t, period) / period < 0.5) {
                double fade_l = 1.0 - u / 0.6;
                if (fade_l < 0.0) fade_l = 0.0;
                *gate_l = (float) fade_l;
                *gate_r = (float) (1.0 - u);
            }
            break;
        }

        case ENDING_PING_PONG_OUT: {
            // Eighth notes, one to each side in turn, every one quieter than the one before
            const double period = beat * 0.5;
            const int k = (int) (t / period);
            if (t - k * period < period * 0.6) {
                const float level = (float) pow(1.0 - u, 1.5);
                if (k % 2 == 0) *gate_l = level; else *gate_r = level;
            }
            break;
        }

        default:
            break;
    }
}

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
    HalfMouco* self = (HalfMouco*) calloc(1, sizeof(HalfMouco));

    if (self == NULL) {
        return NULL;
    }

    self->sample_rate = (float) rate;
    reset(self);

    return (LV2_Handle) self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    HalfMouco* self = (HalfMouco*) instance;

    switch ((PortIndex) port) {
        case AUDIO_IN_L:  self->in_l  = (const float*) data; break;
        case AUDIO_IN_R:  self->in_r  = (const float*) data; break;
        case AUDIO_OUT_L: self->out_l = (float*) data;       break;
        case AUDIO_OUT_R: self->out_r = (float*) data;       break;
        case MODE:    self->mode    = (const float*) data; break;
        case PATTERN: self->pattern = (const float*) data; break;
        case ENDING:  self->ending  = (const float*) data; break;
        case BPM:     self->bpm     = (const float*) data; break;
        case SPEED:   self->speed   = (const float*) data; break;
        case LENGTH:  self->length  = (const float*) data; break;
        case DEPTH:   self->depth   = (const float*) data; break;
        case EDGE:    self->edge    = (const float*) data; break;
        case MONO:    self->mono    = (const float*) data; break;
        case CUSTOM_LENGTH:          self->custom_length          = (const float*) data; break;
        case CUSTOM_STEPS_PER_BEAT:  self->custom_steps_per_beat  = (const float*) data; break;
        case ENABLED: self->enabled = (const float*) data; break;
        default:
            if (port >= CUSTOM_LEFT_FIRST && port < CUSTOM_LEFT_FIRST + CUSTOM_MAX_STEPS) {
                self->custom_left[port - CUSTOM_LEFT_FIRST] = (const float*) data;
            } else if (port >= CUSTOM_RIGHT_FIRST && port < CUSTOM_RIGHT_FIRST + CUSTOM_MAX_STEPS) {
                self->custom_right[port - CUSTOM_RIGHT_FIRST] = (const float*) data;
            }
            break;
    }
}

static void activate(LV2_Handle instance) {
    reset((HalfMouco*) instance);
}

static void run(LV2_Handle instance, uint32_t frames) {
    HalfMouco* self = (HalfMouco*) instance;

    // Nothing to do (or to write to) until the host has connected the audio ports
    if (self->in_l == NULL || self->out_l == NULL || self->out_r == NULL) {
        return;
    }

    const bool  bypassed  = self->enabled != NULL && *self->enabled < 0.5f;
    const bool  endings   = choice(self->mode, 2) == 1;
    const bool  mono      = self->mono != NULL && *self->mono >= 0.5f;
    const float* in_l = self->in_l;
    // Without a right input (or in mono), the left one feeds both sides
    const float* in_r = (mono || self->in_r == NULL) ? self->in_l : self->in_r;

    const float bpm        = clampf(self->bpm ? *self->bpm : 120.0f, BPM_MIN, BPM_MAX);
    const float floor_gain = 1.0f - clampf(self->depth ? *self->depth * 0.01f : 1.0f, 0.0f, 1.0f);
    const float edge_ms    = clampf(self->edge ? *self->edge : 1.5f, EDGE_MIN_MS, EDGE_MAX_MS);

    // The gain of each level: a cut is as deep as Depth says, the middle level halfway between a cut and the whole signal
    const float level_gain[LEVEL_COUNT] = { floor_gain, floor_gain + (1.0f - floor_gain) * 0.5f, 1.0f };

    // The steps of the pattern of each side, one character per step: '0'..'2' (a built-in one points into PATTERNS)
    char        custom_left[CUSTOM_MAX_STEPS + 1];
    char        custom_right[CUSTOM_MAX_STEPS + 1];
    const char* steps_l = "2";
    const char* steps_r = "2";
    int         steps_count = 1;
    double      step_length = 1.0;

    // The ending: how long it lasts and how long a beat is
    const double beat = 60.0 / (double) bpm;
    const double total = (double) clampf(self->length ? *self->length : 8.0f, LENGTH_MIN, LENGTH_MAX) * beat;
    const Ending ending = (Ending) choice(self->ending, ENDING_COUNT);
    EndingState ending_state = { 0 };

    if (!endings) {
        const int pattern_index = choice(self->pattern, PATTERN_COUNT);
        int steps_per_beat;

        if (pattern_index == CUSTOM_PATTERN) {
            steps_count = (int) clampf(self->custom_length ? *self->custom_length + 0.5f : 8.0f, 1.0f, (float) CUSTOM_MAX_STEPS);
            steps_per_beat = (int) clampf(self->custom_steps_per_beat ? *self->custom_steps_per_beat + 0.5f : 2.0f, 1.0f, (float) CUSTOM_MAX_STEPS_PER_BEAT);

            for (int i = 0; i < steps_count; ++i) {
                custom_left[i] = (char) ('0' + choice(self->custom_left[i], LEVEL_COUNT));
                custom_right[i] = (char) ('0' + choice(self->custom_right[i], LEVEL_COUNT));
            }
            custom_left[steps_count] = custom_right[steps_count] = '\0';
            steps_l = custom_left;
            steps_r = custom_right;
        } else {
            steps_l = PATTERNS[pattern_index].left;
            steps_r = PATTERNS[pattern_index].right;
            steps_count = (int) strlen(steps_l);
            steps_per_beat = PATTERNS[pattern_index].steps_per_beat;
        }

        const float multiplier = SPEED_MULTIPLIER[choice(self->speed, SPEED_COUNT)];
        step_length = 60.0 * (double) self->sample_rate / ((double) bpm * (double) steps_per_beat * (double) multiplier);
        if (step_length < 2.0) {
            step_length = 2.0;
        }
    }

    // Slew per sample: the Edge time while chopping, a fixed 10 ms back to the dry signal on bypass
    const float slew_chop   = 1.0f / (edge_ms * 0.001f * self->sample_rate);
    const float slew_bypass = 1.0f / (BYPASS_RAMP_SECONDS * self->sample_rate);

    // Switched on, or another family or another ending: it starts now, with its first step
    if ((!bypassed && self->was_bypassed) || (int) endings != self->last_mode || (int) ending != self->last_ending) {
        self->position = 0.0;
        self->step = 0;
        self->elapsed = 0.0;
    }
    self->was_bypassed = bypassed;
    self->last_mode = (int) endings;
    self->last_ending = (int) ending;

    // The pattern was changed to a shorter one
    if (self->step >= steps_count) {
        self->step = 0;
    }

    const double dt = 1.0 / (double) self->sample_rate;

    for (uint32_t i = 0; i < frames; ++i) {
        float target_l = 1.0f;
        float target_r = 1.0f;
        float slew = slew_bypass;

        if (!bypassed) {
            if (endings) {
                float gate_l, gate_r;
                ending_gate(ending, self->elapsed, total, beat, &ending_state, &gate_l, &gate_r);
                target_l = floor_gain + (1.0f - floor_gain) * gate_l;
                target_r = floor_gain + (1.0f - floor_gain) * gate_r;
            } else {
                target_l = level_gain[steps_l[self->step] - '0'];
                target_r = level_gain[steps_r[self->step] - '0'];
            }
            slew = slew_chop;
        }

        if (self->gain_l < target_l) {
            self->gain_l += slew;
            if (self->gain_l > target_l) self->gain_l = target_l;
        } else if (self->gain_l > target_l) {
            self->gain_l -= slew;
            if (self->gain_l < target_l) self->gain_l = target_l;
        }
        if (self->gain_r < target_r) {
            self->gain_r += slew;
            if (self->gain_r > target_r) self->gain_r = target_r;
        } else if (self->gain_r > target_r) {
            self->gain_r -= slew;
            if (self->gain_r < target_r) self->gain_r = target_r;
        }

        self->out_l[i] = in_l[i] * self->gain_l;
        self->out_r[i] = in_r[i] * self->gain_r;

        // The clock only runs while the effect is on
        if (!bypassed) {
            if (endings) {
                self->elapsed += dt;
            } else {
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
}

static void deactivate(LV2_Handle instance) {}

static void cleanup(LV2_Handle instance) {
    free(instance);
}

static const void* extension_data(const char* uri) {
    return NULL;
}

static const LV2_Descriptor descriptor = {
    HALF_MOUCO_URI,
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
