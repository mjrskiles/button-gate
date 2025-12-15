#ifndef GK_SIM_CV_SOURCE_H
#define GK_SIM_CV_SOURCE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file cv_source.h
 * @brief CV signal generators for simulator
 *
 * Provides LFO, envelope, wavetable, and manual CV sources.
 * The simulator owns timing - frontends send parameters, sim generates samples.
 */

// LFO waveform shapes
typedef enum {
    LFO_SINE,
    LFO_TRI,
    LFO_SAW,
    LFO_SQUARE,
    LFO_RANDOM,     // Sample-and-hold
    LFO_SHAPE_COUNT
} LFOShape;

// Envelope states
typedef enum {
    ENV_IDLE,
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE
} EnvelopeState;

// CV source types
typedef enum {
    CV_SOURCE_MANUAL,
    CV_SOURCE_LFO,
    CV_SOURCE_ENVELOPE,
    CV_SOURCE_WAVETABLE,
    CV_SOURCE_COUNT
} CVSourceType;

// LFO parameters
typedef struct {
    float freq_hz;          // 0.01 - 100 Hz
    LFOShape shape;
    uint8_t min_val;        // Output minimum (0-255)
    uint8_t max_val;        // Output maximum (0-255)
    // Internal state
    float phase;            // 0.0 - 1.0
    float random_value;     // For sample-and-hold
} LFOParams;

// ADSR envelope parameters
typedef struct {
    uint16_t attack_ms;
    uint16_t decay_ms;
    uint8_t sustain;        // Sustain level (0-255)
    uint16_t release_ms;
    // Internal state
    EnvelopeState state;
    uint32_t state_start_ms;
    uint8_t level;          // Current output level
    uint8_t release_level;  // Level when release started
    bool gate;
} EnvelopeParams;

// Wavetable parameters
#define CV_WAVETABLE_MAX_SAMPLES 4096

typedef struct {
    uint8_t *samples;       // Dynamically allocated
    uint16_t length;        // Number of samples
    float freq_hz;          // Playback rate
    // Internal state
    float position;         // Current position (fractional)
} WavetableParams;

// Main CV source struct
typedef struct {
    CVSourceType type;
    uint32_t time_ms;       // Internal accumulated time for timing
    union {
        uint8_t manual_value;
        LFOParams lfo;
        EnvelopeParams envelope;
        WavetableParams wavetable;
    };
} CVSource;

/**
 * Initialize CV source to manual mode with value 0.
 */
void cv_source_init(CVSource *src);

/**
 * Clean up resources (frees wavetable buffer if allocated).
 */
void cv_source_cleanup(CVSource *src);

/**
 * Set manual (fixed) CV value.
 */
void cv_source_set_manual(CVSource *src, uint8_t value);

/**
 * Configure LFO source.
 * @param freq_hz  Frequency in Hz (0.01 - 100)
 * @param shape    Waveform shape
 * @param min_val  Output minimum (0-255)
 * @param max_val  Output maximum (0-255)
 */
void cv_source_set_lfo(CVSource *src, float freq_hz, LFOShape shape,
                       uint8_t min_val, uint8_t max_val);

/**
 * Configure envelope source.
 * @param attack_ms   Attack time in milliseconds
 * @param decay_ms    Decay time in milliseconds
 * @param sustain     Sustain level (0-255)
 * @param release_ms  Release time in milliseconds
 */
void cv_source_set_envelope(CVSource *src, uint16_t attack_ms, uint16_t decay_ms,
                            uint8_t sustain, uint16_t release_ms);

/**
 * Configure wavetable source.
 * @param samples   Sample data (copied internally)
 * @param length    Number of samples (max CV_WAVETABLE_MAX_SAMPLES)
 * @param freq_hz   Playback frequency in Hz
 * @return true on success, false if allocation fails or length exceeds max
 */
bool cv_source_set_wavetable(CVSource *src, const uint8_t *samples,
                             uint16_t length, float freq_hz);

/**
 * Process one tick and return current CV value.
 * @param delta_ms  Time elapsed since last tick (typically 1ms)
 * @return CV value (0-255)
 */
uint8_t cv_source_tick(CVSource *src, uint32_t delta_ms);

/**
 * Envelope gate on (start attack phase).
 * Only affects envelope source type.
 */
void cv_source_gate_on(CVSource *src);

/**
 * Envelope gate off (start release phase).
 * Only affects envelope source type.
 */
void cv_source_gate_off(CVSource *src);

/**
 * Trigger envelope (gate on then immediate gate off).
 * Useful for one-shot envelopes.
 */
void cv_source_trigger(CVSource *src);

/**
 * Reset phase/position to start of cycle.
 */
void cv_source_reset_phase(CVSource *src);

/**
 * Get current source type.
 */
CVSourceType cv_source_get_type(const CVSource *src);

/**
 * Get LFO phase (0.0 - 1.0) for display purposes.
 * Returns 0 if not LFO type.
 */
float cv_source_get_lfo_phase(const CVSource *src);

/**
 * Get envelope state for display purposes.
 * Returns ENV_IDLE if not envelope type.
 */
EnvelopeState cv_source_get_envelope_state(const CVSource *src);

/**
 * Get source type as string.
 */
const char* cv_source_type_str(CVSourceType type);

/**
 * Get LFO shape as string.
 */
const char* cv_source_lfo_shape_str(LFOShape shape);

/**
 * Get envelope state as string.
 */
const char* cv_source_envelope_state_str(EnvelopeState state);

#endif /* GK_SIM_CV_SOURCE_H */
