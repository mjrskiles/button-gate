#include "modes/mode_handlers.h"
#include "core/states.h"
#include "hardware/hal_interface.h"
#include "app_init.h"
#include "config/mode_config.h"
#include "utility/progmem.h"

/**
 * @file mode_handlers.c
 * @brief Mode handler implementations
 *
 * Each mode processes input differently:
 * - Gate: Output follows input directly
 * - Trigger: Rising edge generates fixed-duration pulse
 * - Toggle: Rising edge flips output state
 * - Divide: Output pulse every N input pulses
 * - Cycle: Internal clock at fixed BPM
 */

// =============================================================================
// Gate Mode
// =============================================================================

static void gate_init(GateContext *ctx) {
    ctx->output_state = false;
}

static bool gate_process(GateContext *ctx, bool input, bool *output) {
    bool changed = (ctx->output_state != input);
    ctx->output_state = input;
    *output = input;
    return changed;
}

static void gate_get_led(const GateContext *ctx, LEDFeedback *fb) {
    // Mode LED: Green
    fb->mode_r = LED_COLOR_GATE_R;
    fb->mode_g = LED_COLOR_GATE_G;
    fb->mode_b = LED_COLOR_GATE_B;

    // Activity LED: Mirrors output state
    fb->activity_brightness = ctx->output_state ? 255 : 0;
    fb->activity_r = LED_ACTIVITY_R;
    fb->activity_g = LED_ACTIVITY_G;
    fb->activity_b = LED_ACTIVITY_B;
}

// =============================================================================
// Trigger Mode
// =============================================================================

static void trigger_init(TriggerContext *ctx, const AppSettings *settings) {
    ctx->output_state = false;
    ctx->last_input = false;
    ctx->pulse_start = 0;

    // Get pulse duration from settings
    if (settings && settings->trigger_pulse_idx < TRIGGER_PULSE_COUNT) {
        ctx->pulse_duration_ms = PROGMEM_READ_WORD(&TRIGGER_PULSE_VALUES[settings->trigger_pulse_idx]);
    } else {
        ctx->pulse_duration_ms = TRIGGER_PULSE_DEFAULT;
    }
}

static bool trigger_process(TriggerContext *ctx, bool input, bool *output) {
    bool changed = false;
    uint32_t now = p_hal->millis();

    // Detect rising edge -> start pulse
    if (input && !ctx->last_input) {
        ctx->output_state = true;
        ctx->pulse_start = now;
        changed = true;
    }

    // Check pulse expiry
    if (ctx->output_state) {
        if (now - ctx->pulse_start >= ctx->pulse_duration_ms) {
            ctx->output_state = false;
            changed = true;
        }
    }

    ctx->last_input = input;
    *output = ctx->output_state;
    return changed;
}

static void trigger_get_led(const TriggerContext *ctx, LEDFeedback *fb) {
    // Mode LED: Cyan
    fb->mode_r = LED_COLOR_TRIGGER_R;
    fb->mode_g = LED_COLOR_TRIGGER_G;
    fb->mode_b = LED_COLOR_TRIGGER_B;

    // Activity LED: Mirrors output pulse
    fb->activity_brightness = ctx->output_state ? 255 : 0;
    fb->activity_r = LED_ACTIVITY_R;
    fb->activity_g = LED_ACTIVITY_G;
    fb->activity_b = LED_ACTIVITY_B;
}

// =============================================================================
// Toggle Mode
// =============================================================================

static void toggle_init(ToggleContext *ctx) {
    ctx->output_state = false;
    ctx->last_input = false;
}

static bool toggle_process(ToggleContext *ctx, bool input, bool *output) {
    bool changed = false;

    // Toggle on rising edge
    if (input && !ctx->last_input) {
        ctx->output_state = !ctx->output_state;
        changed = true;
    }

    ctx->last_input = input;
    *output = ctx->output_state;
    return changed;
}

static void toggle_get_led(const ToggleContext *ctx, LEDFeedback *fb) {
    // Mode LED: Orange
    fb->mode_r = LED_COLOR_TOGGLE_R;
    fb->mode_g = LED_COLOR_TOGGLE_G;
    fb->mode_b = LED_COLOR_TOGGLE_B;

    // Activity LED: Shows latched state
    fb->activity_brightness = ctx->output_state ? 255 : 0;
    fb->activity_r = LED_ACTIVITY_R;
    fb->activity_g = LED_ACTIVITY_G;
    fb->activity_b = LED_ACTIVITY_B;
}

// =============================================================================
// Divide Mode
// =============================================================================

static void divide_init(DivideContext *ctx, const AppSettings *settings) {
    ctx->output_state = false;
    ctx->last_input = false;
    ctx->counter = 0;
    ctx->pulse_start = 0;

    // Get divisor from settings
    if (settings && settings->divide_divisor_idx < DIVIDE_DIVISOR_COUNT) {
        ctx->divisor = PROGMEM_READ_BYTE(&DIVIDE_DIVISOR_VALUES[settings->divide_divisor_idx]);
    } else {
        ctx->divisor = DIVIDE_DEFAULT;
    }
}

static bool divide_process(DivideContext *ctx, bool input, bool *output) {
    bool changed = false;
    uint32_t now = p_hal->millis();

    // Count rising edges
    if (input && !ctx->last_input) {
        ctx->counter++;
        if (ctx->counter >= ctx->divisor) {
            ctx->counter = 0;
            ctx->output_state = true;
            ctx->pulse_start = now;
            changed = true;
        }
    }

    // Pulse expiry (short pulse on divided output)
    if (ctx->output_state) {
        if (now - ctx->pulse_start >= OUTPUT_PULSE_MS) {
            ctx->output_state = false;
            changed = true;
        }
    }

    ctx->last_input = input;
    *output = ctx->output_state;
    return changed;
}

static void divide_get_led(const DivideContext *ctx, LEDFeedback *fb) {
    // Mode LED: Magenta
    fb->mode_r = LED_COLOR_DIVIDE_R;
    fb->mode_g = LED_COLOR_DIVIDE_G;
    fb->mode_b = LED_COLOR_DIVIDE_B;

    // Activity LED: Flash on divided output
    fb->activity_brightness = ctx->output_state ? 255 : 0;
    fb->activity_r = LED_ACTIVITY_R;
    fb->activity_g = LED_ACTIVITY_G;
    fb->activity_b = LED_ACTIVITY_B;
}

// =============================================================================
// Cycle Mode
// =============================================================================

static void cycle_init(CycleContext *ctx, const AppSettings *settings) {
    ctx->output_state = false;
    ctx->running = true;  // Start running immediately
    ctx->last_toggle = 0;
    ctx->phase = 0;

    // Get period from settings
    if (settings && settings->cycle_tempo_idx < CYCLE_TEMPO_COUNT) {
        ctx->period_ms = PROGMEM_READ_WORD(&CYCLE_PERIOD_VALUES[settings->cycle_tempo_idx]);
    } else {
        ctx->period_ms = CYCLE_DEFAULT_PERIOD_MS;
    }
}

static bool cycle_process(CycleContext *ctx, bool input, bool *output) {
    (void)input;  // Cycle mode ignores input (for now - tap tempo later)

    if (!ctx->running) {
        *output = false;
        return false;
    }

    bool changed = false;
    uint32_t now = p_hal->millis();
    uint16_t half_period = ctx->period_ms / 2;

    // Toggle output at half-period intervals (square wave)
    if (now - ctx->last_toggle >= half_period) {
        ctx->last_toggle = now;
        ctx->output_state = !ctx->output_state;
        changed = true;
    }

    // Update phase for LED animation (0-255 over full period)
    // Phase represents position in cycle: 0 = start, 128 = middle, 255 = end
    uint32_t elapsed = now - (ctx->last_toggle - (ctx->output_state ? 0 : half_period));
    uint32_t cycle_pos = elapsed % ctx->period_ms;
    ctx->phase = (uint8_t)((cycle_pos * 255) / ctx->period_ms);

    *output = ctx->output_state;
    return changed;
}

static void cycle_get_led(const CycleContext *ctx, LEDFeedback *fb) {
    // Mode LED: Yellow
    fb->mode_r = LED_COLOR_CYCLE_R;
    fb->mode_g = LED_COLOR_CYCLE_G;
    fb->mode_b = LED_COLOR_CYCLE_B;

    // Activity LED: Pulsing brightness following cycle phase
    // Triangle wave: ramp up 0-127, ramp down 128-255
    uint8_t brightness;
    if (ctx->phase < 128) {
        brightness = ctx->phase * 2;
    } else {
        brightness = (255 - ctx->phase) * 2;
    }
    fb->activity_brightness = brightness;
    fb->activity_r = LED_ACTIVITY_R;
    fb->activity_g = LED_ACTIVITY_G;
    fb->activity_b = LED_ACTIVITY_B;
}

// =============================================================================
// Public API - Switch-based dispatch
// =============================================================================

void mode_handler_init(uint8_t mode, ModeContext *ctx, const AppSettings *settings) {
    if (!ctx) return;

    switch (mode) {
        case MODE_GATE:
            gate_init(&ctx->gate);
            break;
        case MODE_TRIGGER:
            trigger_init(&ctx->trigger, settings);
            break;
        case MODE_TOGGLE:
            toggle_init(&ctx->toggle);
            break;
        case MODE_DIVIDE:
            divide_init(&ctx->divide, settings);
            break;
        case MODE_CYCLE:
            cycle_init(&ctx->cycle, settings);
            break;
        default:
            gate_init(&ctx->gate);
            break;
    }
}

bool mode_handler_process(uint8_t mode, ModeContext *ctx, bool input, bool *output) {
    if (!ctx || !output) return false;

    switch (mode) {
        case MODE_GATE:
            return gate_process(&ctx->gate, input, output);
        case MODE_TRIGGER:
            return trigger_process(&ctx->trigger, input, output);
        case MODE_TOGGLE:
            return toggle_process(&ctx->toggle, input, output);
        case MODE_DIVIDE:
            return divide_process(&ctx->divide, input, output);
        case MODE_CYCLE:
            return cycle_process(&ctx->cycle, input, output);
        default:
            return gate_process(&ctx->gate, input, output);
    }
}

void mode_handler_get_led(uint8_t mode, const ModeContext *ctx, LEDFeedback *fb) {
    if (!ctx || !fb) return;

    switch (mode) {
        case MODE_GATE:
            gate_get_led(&ctx->gate, fb);
            break;
        case MODE_TRIGGER:
            trigger_get_led(&ctx->trigger, fb);
            break;
        case MODE_TOGGLE:
            toggle_get_led(&ctx->toggle, fb);
            break;
        case MODE_DIVIDE:
            divide_get_led(&ctx->divide, fb);
            break;
        case MODE_CYCLE:
            cycle_get_led(&ctx->cycle, fb);
            break;
        default:
            gate_get_led(&ctx->gate, fb);
            break;
    }
}
