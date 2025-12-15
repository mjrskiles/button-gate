#include "cv_source.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @file cv_source.c
 * @brief CV signal generator implementations
 */

// String tables
static const char *source_type_names[] = {
    "manual", "lfo", "envelope", "wavetable"
};

static const char *lfo_shape_names[] = {
    "sine", "tri", "saw", "square", "random"
};

static const char *envelope_state_names[] = {
    "idle", "attack", "decay", "sustain", "release"
};

// =============================================================================
// LFO Implementation
// =============================================================================

/**
 * Get LFO waveform value for given phase.
 * @param shape  Waveform type
 * @param phase  Phase position (0.0 - 1.0)
 * @param random_val  Pointer to random value (for S&H, updated on phase wrap)
 * @return Normalized value (-1.0 to 1.0)
 */
static float lfo_shape_value(LFOShape shape, float phase, float *random_val) {
    switch (shape) {
        case LFO_SINE:
            return sinf(phase * 2.0f * (float)M_PI);

        case LFO_TRI:
            // Triangle: rises 0->1 in first half, falls 1->0 in second half
            // Normalized to -1..1
            if (phase < 0.5f) {
                return 4.0f * phase - 1.0f;
            } else {
                return 3.0f - 4.0f * phase;
            }

        case LFO_SAW:
            // Sawtooth: rises from -1 to 1 over the cycle
            return 2.0f * phase - 1.0f;

        case LFO_SQUARE:
            return (phase < 0.5f) ? 1.0f : -1.0f;

        case LFO_RANDOM:
            // Sample-and-hold: return stored random value
            // (updated on phase wrap in tick function)
            return *random_val;

        default:
            return 0.0f;
    }
}

/**
 * Process LFO tick.
 */
static uint8_t lfo_tick(LFOParams *lfo, uint32_t delta_ms) {
    // Advance phase
    float phase_inc = (lfo->freq_hz * (float)delta_ms) / 1000.0f;
    lfo->phase += phase_inc;

    // Handle phase wrap
    while (lfo->phase >= 1.0f) {
        lfo->phase -= 1.0f;
        // Generate new random value on wrap (for S&H)
        if (lfo->shape == LFO_RANDOM) {
            lfo->random_value = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        }
    }

    // Get normalized value (-1 to 1)
    float normalized = lfo_shape_value(lfo->shape, lfo->phase, &lfo->random_value);

    // Scale to 0..1
    float scaled = (normalized + 1.0f) * 0.5f;

    // Map to min..max range
    float range = (float)(lfo->max_val - lfo->min_val);
    return lfo->min_val + (uint8_t)(scaled * range);
}

// =============================================================================
// Envelope Implementation
// =============================================================================

/**
 * Process envelope tick.
 */
static uint8_t envelope_tick(EnvelopeParams *env, uint32_t current_time_ms) {
    uint32_t elapsed = current_time_ms - env->state_start_ms;

    switch (env->state) {
        case ENV_IDLE:
            env->level = 0;
            break;

        case ENV_ATTACK:
            if (env->attack_ms == 0) {
                env->level = 255;
                env->state = ENV_DECAY;
                env->state_start_ms = current_time_ms;
            } else {
                float progress = (float)elapsed / (float)env->attack_ms;
                if (progress >= 1.0f) {
                    env->level = 255;
                    env->state = ENV_DECAY;
                    env->state_start_ms = current_time_ms;
                } else {
                    env->level = (uint8_t)(progress * 255.0f);
                }
            }
            break;

        case ENV_DECAY:
            if (env->decay_ms == 0) {
                env->level = env->sustain;
                env->state = ENV_SUSTAIN;
                env->state_start_ms = current_time_ms;
            } else {
                float progress = (float)elapsed / (float)env->decay_ms;
                if (progress >= 1.0f) {
                    env->level = env->sustain;
                    env->state = ENV_SUSTAIN;
                    env->state_start_ms = current_time_ms;
                } else {
                    // Decay from 255 to sustain level
                    float range = 255.0f - (float)env->sustain;
                    env->level = 255 - (uint8_t)(progress * range);
                }
            }
            break;

        case ENV_SUSTAIN:
            env->level = env->sustain;
            // Stay here until gate off
            break;

        case ENV_RELEASE:
            if (env->release_ms == 0) {
                env->level = 0;
                env->state = ENV_IDLE;
            } else {
                float progress = (float)elapsed / (float)env->release_ms;
                if (progress >= 1.0f) {
                    env->level = 0;
                    env->state = ENV_IDLE;
                } else {
                    // Release from release_level to 0
                    env->level = env->release_level - (uint8_t)(progress * (float)env->release_level);
                }
            }
            break;
    }

    return env->level;
}

/**
 * Start envelope attack phase.
 */
static void envelope_gate_on_internal(EnvelopeParams *env, uint32_t current_time_ms) {
    env->gate = true;
    env->state = ENV_ATTACK;
    env->state_start_ms = current_time_ms;
}

/**
 * Start envelope release phase.
 */
static void envelope_gate_off_internal(EnvelopeParams *env, uint32_t current_time_ms) {
    if (env->state != ENV_IDLE && env->state != ENV_RELEASE) {
        env->gate = false;
        env->release_level = env->level;
        env->state = ENV_RELEASE;
        env->state_start_ms = current_time_ms;
    }
}

// =============================================================================
// Wavetable Implementation
// =============================================================================

/**
 * Process wavetable tick with linear interpolation.
 */
static uint8_t wavetable_tick(WavetableParams *wt, uint32_t delta_ms) {
    if (!wt->samples || wt->length == 0) {
        return 0;
    }

    // Get interpolated sample
    uint16_t idx0 = (uint16_t)wt->position;
    uint16_t idx1 = (idx0 + 1) % wt->length;
    float frac = wt->position - (float)idx0;

    float sample = (1.0f - frac) * (float)wt->samples[idx0] +
                   frac * (float)wt->samples[idx1];

    // Advance position
    float pos_inc = (wt->freq_hz * (float)wt->length * (float)delta_ms) / 1000.0f;
    wt->position += pos_inc;

    // Wrap position
    while (wt->position >= (float)wt->length) {
        wt->position -= (float)wt->length;
    }

    return (uint8_t)sample;
}

// =============================================================================
// Public API
// =============================================================================

void cv_source_init(CVSource *src) {
    if (!src) return;
    memset(src, 0, sizeof(CVSource));
    src->type = CV_SOURCE_MANUAL;
    src->time_ms = 0;
    src->manual_value = 0;
}

void cv_source_cleanup(CVSource *src) {
    if (!src) return;
    if (src->type == CV_SOURCE_WAVETABLE && src->wavetable.samples) {
        free(src->wavetable.samples);
        src->wavetable.samples = NULL;
    }
}

void cv_source_set_manual(CVSource *src, uint8_t value) {
    if (!src) return;
    cv_source_cleanup(src);
    src->type = CV_SOURCE_MANUAL;
    src->manual_value = value;
}

void cv_source_set_lfo(CVSource *src, float freq_hz, LFOShape shape,
                       uint8_t min_val, uint8_t max_val) {
    if (!src) return;
    cv_source_cleanup(src);

    src->type = CV_SOURCE_LFO;
    src->lfo.freq_hz = freq_hz;
    src->lfo.shape = shape;
    src->lfo.min_val = min_val;
    src->lfo.max_val = max_val;
    src->lfo.phase = 0.0f;
    src->lfo.random_value = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
}

void cv_source_set_envelope(CVSource *src, uint16_t attack_ms, uint16_t decay_ms,
                            uint8_t sustain, uint16_t release_ms) {
    if (!src) return;
    cv_source_cleanup(src);

    src->type = CV_SOURCE_ENVELOPE;
    src->envelope.attack_ms = attack_ms;
    src->envelope.decay_ms = decay_ms;
    src->envelope.sustain = sustain;
    src->envelope.release_ms = release_ms;
    src->envelope.state = ENV_IDLE;
    src->envelope.state_start_ms = 0;
    src->envelope.level = 0;
    src->envelope.release_level = 0;
    src->envelope.gate = false;
}

bool cv_source_set_wavetable(CVSource *src, const uint8_t *samples,
                             uint16_t length, float freq_hz) {
    if (!src || !samples || length == 0) return false;
    if (length > CV_WAVETABLE_MAX_SAMPLES) return false;

    cv_source_cleanup(src);

    // Allocate and copy samples
    uint8_t *buf = (uint8_t *)malloc(length);
    if (!buf) return false;
    memcpy(buf, samples, length);

    src->type = CV_SOURCE_WAVETABLE;
    src->wavetable.samples = buf;
    src->wavetable.length = length;
    src->wavetable.freq_hz = freq_hz;
    src->wavetable.position = 0.0f;

    return true;
}

uint8_t cv_source_tick(CVSource *src, uint32_t delta_ms) {
    if (!src) return 0;

    // Accumulate time
    src->time_ms += delta_ms;

    switch (src->type) {
        case CV_SOURCE_MANUAL:
            return src->manual_value;

        case CV_SOURCE_LFO:
            return lfo_tick(&src->lfo, delta_ms);

        case CV_SOURCE_ENVELOPE:
            return envelope_tick(&src->envelope, src->time_ms);

        case CV_SOURCE_WAVETABLE:
            return wavetable_tick(&src->wavetable, delta_ms);

        default:
            return 0;
    }
}

void cv_source_gate_on(CVSource *src) {
    if (!src || src->type != CV_SOURCE_ENVELOPE) return;
    envelope_gate_on_internal(&src->envelope, src->time_ms);
}

void cv_source_gate_off(CVSource *src) {
    if (!src || src->type != CV_SOURCE_ENVELOPE) return;
    envelope_gate_off_internal(&src->envelope, src->time_ms);
}

void cv_source_trigger(CVSource *src) {
    if (!src || src->type != CV_SOURCE_ENVELOPE) return;
    envelope_gate_on_internal(&src->envelope, src->time_ms);
    // For trigger mode, we don't immediately gate off -
    // the envelope runs through attack/decay to sustain=0 naturally
    // if sustain is 0, or caller can gate off later
}

void cv_source_reset_phase(CVSource *src) {
    if (!src) return;

    switch (src->type) {
        case CV_SOURCE_LFO:
            src->lfo.phase = 0.0f;
            break;
        case CV_SOURCE_WAVETABLE:
            src->wavetable.position = 0.0f;
            break;
        case CV_SOURCE_ENVELOPE:
            src->envelope.state = ENV_IDLE;
            src->envelope.level = 0;
            break;
        default:
            break;
    }
}

CVSourceType cv_source_get_type(const CVSource *src) {
    if (!src) return CV_SOURCE_MANUAL;
    return src->type;
}

float cv_source_get_lfo_phase(const CVSource *src) {
    if (!src || src->type != CV_SOURCE_LFO) return 0.0f;
    return src->lfo.phase;
}

EnvelopeState cv_source_get_envelope_state(const CVSource *src) {
    if (!src || src->type != CV_SOURCE_ENVELOPE) return ENV_IDLE;
    return src->envelope.state;
}

const char* cv_source_type_str(CVSourceType type) {
    if (type >= CV_SOURCE_COUNT) return "unknown";
    return source_type_names[type];
}

const char* cv_source_lfo_shape_str(LFOShape shape) {
    if (shape >= LFO_SHAPE_COUNT) return "unknown";
    return lfo_shape_names[shape];
}

const char* cv_source_envelope_state_str(EnvelopeState state) {
    if (state > ENV_RELEASE) return "unknown";
    return envelope_state_names[state];
}
