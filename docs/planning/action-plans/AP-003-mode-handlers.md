# AP-003: Mode Handlers

**Status: Planned**

## Overview

Implement the five mode handlers (Gate, Trigger, Toggle, Divide, Cycle) that process CV/button input and generate output signals. Each handler also provides mode-specific LED feedback via the `get_led_feedback()` callback.

## Prerequisites

- [x] AP-001: FSM Engine (complete)
- [ ] AP-002: Gatekeeper FSM Setup
- [x] FDP-004: State Machine Module (mode handler interface)

## Scope

**In scope:**
- ModeHandler interface implementation
- Gate mode handler (pass-through)
- Trigger mode handler (edge-triggered pulse)
- Toggle mode handler (flip-flop)
- Divide mode handler (clock divider)
- Cycle mode handler (internal LFO/clock)
- LED feedback callbacks for each mode
- Mode-specific context structs
- Unit tests for each handler

**Not in scope:**
- Actual LED driving (AP-004)
- CV input ADC reading (uses abstracted input)
- Menu settings integration (later)

## Implementation Steps

### Phase 1: Handler Interface & Types

**File: `include/fsm/mode_handlers.h`**

```c
#ifndef GK_MODE_HANDLERS_H
#define GK_MODE_HANDLERS_H

#include <stdint.h>
#include <stdbool.h>

/**
 * LED feedback state returned by mode handlers
 */
typedef struct {
    uint8_t led_x_color_r;
    uint8_t led_x_color_g;
    uint8_t led_x_color_b;
    uint8_t led_y_state;        // 0=off, 1=on, 2=blink, 3=glow
    uint8_t led_y_color_r;
    uint8_t led_y_color_g;
    uint8_t led_y_color_b;
} LEDFeedback;

// LED states
#define LED_STATE_OFF   0
#define LED_STATE_ON    1
#define LED_STATE_BLINK 2
#define LED_STATE_GLOW  3

/**
 * Mode handler interface
 */
typedef struct ModeHandler {
    bool (*process)(void *ctx, bool input, bool *output);
    void (*on_activate)(void *ctx);
    void (*on_deactivate)(void *ctx);
    void (*get_led_feedback)(void *ctx, LEDFeedback *feedback);
} ModeHandler;

// Handler instances
extern const ModeHandler gate_handler;
extern const ModeHandler trigger_handler;
extern const ModeHandler toggle_handler;
extern const ModeHandler divide_handler;
extern const ModeHandler cycle_handler;

// Handler lookup by mode index
const ModeHandler* get_mode_handler(uint8_t mode);

#endif
```

### Phase 2: Gate Mode Handler

Simplest mode - output follows input directly.

**Context:**
```c
typedef struct {
    bool output_state;
} GateContext;
```

**Implementation:**
```c
static bool gate_process(void *ctx, bool input, bool *output) {
    GateContext *g = (GateContext *)ctx;
    bool changed = (g->output_state != input);
    g->output_state = input;
    *output = input;
    return changed;
}

static void gate_led_feedback(void *ctx, LEDFeedback *fb) {
    GateContext *g = (GateContext *)ctx;
    // LED X: Green (mode color)
    fb->led_x_color_r = 0;
    fb->led_x_color_g = 255;
    fb->led_x_color_b = 0;
    // LED Y: Mirrors output state
    fb->led_y_state = g->output_state ? LED_STATE_ON : LED_STATE_OFF;
    fb->led_y_color_r = 255;
    fb->led_y_color_g = 255;
    fb->led_y_color_b = 255;
}
```

### Phase 3: Trigger Mode Handler

Generates fixed-duration pulse on rising edge.

**Context:**
```c
typedef struct {
    bool output_state;
    bool last_input;
    bool pulse_active;
    uint32_t pulse_start;
    uint16_t pulse_duration_ms;  // Configurable: 1, 10, 20, 50ms
} TriggerContext;
```

**Implementation:**
```c
static bool trigger_process(void *ctx, bool input, bool *output) {
    TriggerContext *t = (TriggerContext *)ctx;
    bool changed = false;

    // Detect rising edge
    if (input && !t->last_input) {
        t->pulse_active = true;
        t->pulse_start = p_hal->millis();
        t->output_state = true;
        changed = true;
    }

    // Check pulse expiry
    if (t->pulse_active) {
        if (p_hal->millis() - t->pulse_start >= t->pulse_duration_ms) {
            t->pulse_active = false;
            t->output_state = false;
            changed = true;
        }
    }

    t->last_input = input;
    *output = t->output_state;
    return changed;
}
```

### Phase 4: Toggle Mode Handler

Flip-flop that toggles on rising edge.

**Context:**
```c
typedef struct {
    bool output_state;
    bool last_input;
} ToggleContext;
```

**Implementation:**
```c
static bool toggle_process(void *ctx, bool input, bool *output) {
    ToggleContext *t = (ToggleContext *)ctx;
    bool changed = false;

    // Toggle on rising edge
    if (input && !t->last_input) {
        t->output_state = !t->output_state;
        changed = true;
    }

    t->last_input = input;
    *output = t->output_state;
    return changed;
}
```

### Phase 5: Divide Mode Handler

Clock divider - outputs every Nth input pulse.

**Context:**
```c
typedef struct {
    bool output_state;
    bool last_input;
    uint8_t counter;
    uint8_t divisor;            // 2, 4, 8, 16, 24
    bool output_triggered;      // For LED feedback
    uint16_t pulse_duration_ms;
    uint32_t pulse_start;
} DivideContext;
```

**Implementation:**
```c
static bool divide_process(void *ctx, bool input, bool *output) {
    DivideContext *d = (DivideContext *)ctx;
    bool changed = false;
    d->output_triggered = false;

    // Count rising edges
    if (input && !d->last_input) {
        d->counter++;
        if (d->counter >= d->divisor) {
            d->counter = 0;
            d->output_state = true;
            d->output_triggered = true;
            d->pulse_start = p_hal->millis();
            changed = true;
        }
    }

    // Pulse expiry
    if (d->output_state) {
        if (p_hal->millis() - d->pulse_start >= d->pulse_duration_ms) {
            d->output_state = false;
            changed = true;
        }
    }

    d->last_input = input;
    *output = d->output_state;
    return changed;
}

static void divide_led_feedback(void *ctx, LEDFeedback *fb) {
    DivideContext *d = (DivideContext *)ctx;
    // LED X: Magenta
    fb->led_x_color_r = 255;
    fb->led_x_color_g = 0;
    fb->led_x_color_b = 255;
    // LED Y: Flash on divided output only
    fb->led_y_state = d->output_triggered ? LED_STATE_ON : LED_STATE_OFF;
    fb->led_y_color_r = 255;
    fb->led_y_color_g = 255;
    fb->led_y_color_b = 255;
}
```

### Phase 6: Cycle Mode Handler

Internal LFO/clock generator with tap tempo.

**Context:**
```c
typedef struct {
    bool output_state;
    uint32_t last_toggle_time;
    uint32_t period_ms;         // Full cycle period
    uint32_t last_tap_time;     // For tap tempo
    uint8_t phase;              // 0-255 for LED glow
    bool running;
} CycleContext;
```

**Implementation:**
```c
static bool cycle_process(void *ctx, bool input, bool *output) {
    CycleContext *c = (CycleContext *)ctx;
    bool changed = false;

    if (!c->running) {
        *output = false;
        return false;
    }

    uint32_t now = p_hal->millis();
    uint32_t half_period = c->period_ms / 2;

    if (now - c->last_toggle_time >= half_period) {
        c->last_toggle_time = now;
        c->output_state = !c->output_state;
        changed = true;
    }

    // Update phase for LED glow (0-255)
    uint32_t phase_time = now % c->period_ms;
    c->phase = (phase_time * 255) / c->period_ms;

    *output = c->output_state;
    return changed;
}

static void cycle_led_feedback(void *ctx, LEDFeedback *fb) {
    CycleContext *c = (CycleContext *)ctx;
    // LED X: Yellow
    fb->led_x_color_r = 255;
    fb->led_x_color_g = 255;
    fb->led_x_color_b = 0;
    // LED Y: Glow with phase
    fb->led_y_state = LED_STATE_GLOW;
    // Brightness varies with phase
    uint8_t brightness = c->phase < 128 ? c->phase * 2 : (255 - c->phase) * 2;
    fb->led_y_color_r = brightness;
    fb->led_y_color_g = brightness;
    fb->led_y_color_b = brightness;
}
```

### Phase 7: Handler Registration

```c
// mode_handlers.c
static GateContext gate_ctx;
static TriggerContext trigger_ctx;
static ToggleContext toggle_ctx;
static DivideContext divide_ctx;
static CycleContext cycle_ctx;

const ModeHandler gate_handler = {
    .process = gate_process,
    .on_activate = gate_activate,
    .on_deactivate = gate_deactivate,
    .get_led_feedback = gate_led_feedback
};

// ... similar for other handlers ...

static const ModeHandler* handlers[] = {
    &gate_handler,
    &trigger_handler,
    &toggle_handler,
    &divide_handler,
    &cycle_handler
};

const ModeHandler* get_mode_handler(uint8_t mode) {
    if (mode >= MODE_COUNT) return &gate_handler;
    return handlers[mode];
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `include/fsm/mode_handlers.h` | Create | Handler interface & LED feedback types |
| `include/fsm/mode_contexts.h` | Create | Mode-specific context structs |
| `src/fsm/mode_handlers.c` | Create | All handler implementations |
| `test/unit/fsm/test_mode_handlers.h` | Create | Handler unit tests |
| `test/unit/CMakeLists.txt` | Modify | Add new sources |

## Testing Strategy

**Per-handler tests:**
- Gate: Output follows input
- Trigger: Pulse on rising edge, correct duration
- Toggle: Flip-flop behavior
- Divide: Correct division ratios (2, 4, 8, 16, 24)
- Cycle: Correct period, start/stop

**LED feedback tests:**
- Each mode returns correct LED X color
- LED Y state reflects mode state

## Success Criteria

- [ ] All 5 handlers implemented
- [ ] Gate mode passes input through
- [ ] Trigger generates correct pulse duration
- [ ] Toggle flips on rising edge
- [ ] Divide counts correctly for all divisors
- [ ] Cycle generates correct frequency
- [ ] LED feedback correct for each mode
- [ ] All tests pass

## Dependencies

- AP-001: FSM Engine (for integration)
- HAL for timing (millis)

## Notes

- Handlers are pure functions with explicit context
- LED colors defined here, actual driving in AP-004
- Settings (pulse duration, divisor, etc.) will come from menu later
- Cycle mode tap tempo is a future enhancement
