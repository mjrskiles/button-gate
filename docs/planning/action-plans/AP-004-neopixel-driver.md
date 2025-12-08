# AP-004: Neopixel Driver & LED Feedback

**Status: Planned**

## Overview

Implement the WS2812B (Neopixel) driver for ATtiny85 at 8MHz and the LED feedback system that displays mode/menu state. Two LEDs: X (mode/page indicator) and Y (value/state indicator).

## Prerequisites

- [x] AP-001: FSM Engine (complete)
- [ ] AP-002: Gatekeeper FSM Setup
- [ ] AP-003: Mode Handlers (for LED feedback interface)
- [x] FDP-005: Neopixel Driver & LED Feedback (design)

## Scope

**In scope:**
- WS2812B bit-banged driver (cycle-accurate timing)
- LED buffer management (2 LEDs × 3 bytes)
- Animation engine (blink, glow effects)
- LED feedback controller (high-level API)
- Color lookup tables
- Integration with mode handlers

**Not in scope:**
- Hardware testing (requires actual LEDs)
- Color calibration

## Implementation Steps

### Phase 1: Neopixel Driver Core

**File: `include/output/neopixel.h`**

```c
#ifndef GK_NEOPIXEL_H
#define GK_NEOPIXEL_H

#include <stdint.h>
#include <stdbool.h>

#define NEOPIXEL_COUNT 2
#define LED_X 0
#define LED_Y 1

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Color;

void neopixel_init(void);
void neopixel_set_color(uint8_t index, Color color);
void neopixel_set_rgb(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
Color neopixel_get_color(uint8_t index);
void neopixel_clear(void);
void neopixel_flush(void);

#endif
```

**File: `src/output/neopixel.c`**

Core driver with inline assembly for timing:

```c
#include "output/neopixel.h"
#include "hardware/hal.h"

// LED buffer in GRB order (WS2812B native)
static uint8_t led_buffer[NEOPIXEL_COUNT * 3];
static bool buffer_dirty = false;

void neopixel_init(void) {
    // Configure PB0 as output
    DDRB |= (1 << PB0);
    PORTB &= ~(1 << PB0);
    neopixel_clear();
}

void neopixel_set_color(uint8_t index, Color color) {
    if (index >= NEOPIXEL_COUNT) return;
    uint8_t offset = index * 3;
    led_buffer[offset + 0] = color.g;  // GRB order
    led_buffer[offset + 1] = color.r;
    led_buffer[offset + 2] = color.b;
    buffer_dirty = true;
}

// Timing-critical transmission (interrupts disabled)
static void send_byte(uint8_t byte) {
    for (uint8_t bit = 0; bit < 8; bit++) {
        if (byte & 0x80) {
            // Send 1: T1H=750ns (6 cycles), T1L=500ns (4 cycles)
            asm volatile(
                "sbi %[port], %[pin]" "\n\t"
                "nop\nnop\nnop\nnop" "\n\t"  // 4 nops = 4 cycles
                "cbi %[port], %[pin]" "\n\t"
                "nop\nnop" "\n\t"            // 2 nops
                :
                : [port] "I" (_SFR_IO_ADDR(PORTB)),
                  [pin] "I" (PB0)
            );
        } else {
            // Send 0: T0H=375ns (3 cycles), T0L=875ns (7 cycles)
            asm volatile(
                "sbi %[port], %[pin]" "\n\t"
                "nop" "\n\t"                  // 1 nop
                "cbi %[port], %[pin]" "\n\t"
                "nop\nnop\nnop\nnop\nnop" "\n\t"  // 5 nops
                :
                : [port] "I" (_SFR_IO_ADDR(PORTB)),
                  [pin] "I" (PB0)
            );
        }
        byte <<= 1;
    }
}

void neopixel_flush(void) {
    if (!buffer_dirty) return;

    cli();  // Disable interrupts
    for (uint8_t i = 0; i < NEOPIXEL_COUNT * 3; i++) {
        send_byte(led_buffer[i]);
    }
    sei();  // Re-enable interrupts

    _delay_us(50);  // Reset pulse
    buffer_dirty = false;
}
```

### Phase 2: Animation Engine

**File: `include/output/led_animation.h`**

```c
typedef enum {
    ANIM_NONE = 0,
    ANIM_BLINK,
    ANIM_GLOW,
} AnimationType;

typedef struct {
    AnimationType type;
    Color base_color;
    uint32_t period_ms;
    uint32_t last_update;
    uint8_t phase;          // 0-255
    bool current_on;        // For blink
} LEDAnimation;

void led_animation_init(LEDAnimation *anim);
void led_animation_set(LEDAnimation *anim, AnimationType type,
                       Color color, uint32_t period_ms);
void led_animation_update(LEDAnimation *anim, uint8_t led_index,
                          uint32_t current_time);
void led_animation_stop(LEDAnimation *anim, uint8_t led_index);
```

**Implementation:**
```c
void led_animation_update(LEDAnimation *anim, uint8_t led_index,
                          uint32_t current_time) {
    if (anim->type == ANIM_NONE) return;

    switch (anim->type) {
        case ANIM_BLINK:
            if (current_time - anim->last_update >= anim->period_ms / 2) {
                anim->last_update = current_time;
                anim->current_on = !anim->current_on;
                if (anim->current_on) {
                    neopixel_set_color(led_index, anim->base_color);
                } else {
                    neopixel_set_rgb(led_index, 0, 0, 0);
                }
            }
            break;

        case ANIM_GLOW: {
            // Smooth triangle wave
            uint32_t phase_time = current_time % anim->period_ms;
            anim->phase = (phase_time * 255) / anim->period_ms;

            uint8_t brightness;
            if (anim->phase < 128) {
                brightness = anim->phase * 2;
            } else {
                brightness = (255 - anim->phase) * 2;
            }

            // Scale base color by brightness
            Color scaled = {
                (anim->base_color.r * brightness) / 255,
                (anim->base_color.g * brightness) / 255,
                (anim->base_color.b * brightness) / 255
            };
            neopixel_set_color(led_index, scaled);
            break;
        }
    }
}
```

### Phase 3: LED Feedback Controller

**File: `include/output/led_feedback.h`**

```c
#ifndef GK_LED_FEEDBACK_H
#define GK_LED_FEEDBACK_H

#include "fsm/mode_handlers.h"  // For LEDFeedback type

void led_feedback_init(void);
void led_feedback_update(uint32_t current_time);

// Mode/menu display
void led_feedback_show_mode(uint8_t mode);
void led_feedback_show_page(uint8_t page);
void led_feedback_show_value(uint8_t value);

// Apply feedback from mode handler
void led_feedback_apply(const LEDFeedback *fb);

// Direct control
void led_feedback_set_x(uint8_t r, uint8_t g, uint8_t b);
void led_feedback_set_y_state(uint8_t state, uint8_t r, uint8_t g, uint8_t b);

#endif
```

### Phase 4: Color Definitions

**File: `include/output/led_colors.h`**

```c
// Mode colors (LED X in Perform)
#define COLOR_MODE_GATE      (Color){  0, 255,   0}  // Green
#define COLOR_MODE_TRIGGER   (Color){255, 128,   0}  // Orange
#define COLOR_MODE_TOGGLE    (Color){  0, 128, 255}  // Cyan
#define COLOR_MODE_DIVIDE    (Color){255,   0, 255}  // Magenta
#define COLOR_MODE_CYCLE     (Color){255, 255,   0}  // Yellow

// Page colors (LED X in Menu)
#define COLOR_PAGE_GATE      (Color){  0, 255,   0}  // Green
#define COLOR_PAGE_TRIGGER1  (Color){255, 128,   0}  // Orange
#define COLOR_PAGE_TRIGGER2  (Color){255,  64,   0}  // Dark orange
#define COLOR_PAGE_TOGGLE    (Color){  0, 128, 255}  // Cyan
#define COLOR_PAGE_DIVIDE    (Color){255,   0, 255}  // Magenta
#define COLOR_PAGE_CYCLE     (Color){255, 255,   0}  // Yellow
#define COLOR_PAGE_CV        (Color){255, 255, 255}  // White
#define COLOR_PAGE_TIMEOUT   (Color){128, 128, 128}  // Gray

// Lookup tables (PROGMEM on target)
extern const Color mode_colors[5];
extern const Color page_colors[8];

Color led_get_mode_color(uint8_t mode);
Color led_get_page_color(uint8_t page);
```

### Phase 5: Mock Driver for Testing

**File: `test/unit/mocks/mock_neopixel.h`**

```c
// Test-friendly mock that doesn't require AVR
typedef struct {
    uint8_t r, g, b;
} MockLED;

extern MockLED mock_leds[2];
extern int neopixel_flush_count;

void mock_neopixel_reset(void);
```

### Phase 6: Integration

```c
// In gatekeeper_fsm_update()
void gatekeeper_fsm_update(GatekeeperFSM *gk) {
    // ... process events ...

    // Get LED feedback from current mode
    if (fsm_get_state(&gk->top_fsm) == TOP_PERFORM) {
        LEDFeedback fb;
        const ModeHandler *handler = get_mode_handler(
            fsm_get_state(&gk->mode_fsm));
        handler->get_led_feedback(mode_ctx, &fb);
        led_feedback_apply(&fb);
    }

    // Update animations and flush
    led_feedback_update(p_hal->millis());
    neopixel_flush();
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `include/output/neopixel.h` | Create | Driver interface |
| `include/output/led_animation.h` | Create | Animation types |
| `include/output/led_feedback.h` | Create | High-level controller |
| `include/output/led_colors.h` | Create | Color definitions |
| `src/output/neopixel.c` | Create | Bit-banged driver |
| `src/output/led_animation.c` | Create | Animation engine |
| `src/output/led_feedback.c` | Create | Feedback controller |
| `src/output/led_colors.c` | Create | Color lookup tables |
| `test/unit/mocks/mock_neopixel.c` | Create | Mock for testing |
| `test/unit/output/test_led_feedback.h` | Create | LED feedback tests |
| `test/unit/output/test_led_animation.h` | Create | Animation tests |

## Testing Strategy

**Driver tests (with mock):**
- Color setting updates buffer
- GRB byte order correct
- Flush marks buffer clean

**Animation tests:**
- Blink toggles at correct rate
- Glow produces smooth brightness curve

**Feedback tests:**
- Mode colors correct
- Page colors correct
- Value states (off/on/blink/glow) work

## Success Criteria

- [ ] Neopixel driver compiles for AVR
- [ ] Mock driver works for unit tests
- [ ] Blink animation at ~2Hz
- [ ] Glow animation smooth (256 steps)
- [ ] All mode colors distinct
- [ ] All page colors distinct
- [ ] LED Y shows 4 value states
- [ ] All tests pass

## Hardware Testing Notes

Once on hardware:
1. Verify timing with logic analyzer
2. Check color appearance (may need tuning)
3. Verify no flicker during updates
4. Confirm interrupt latency acceptable

## Dependencies

- AP-003: Mode Handlers (LEDFeedback type)
- HAL for millis() timing

## Notes

- Driver requires AVR-specific inline assembly
- Mock driver used for unit tests
- ~60µs interrupt blackout during 2-LED transmission
- Colors may need tuning based on actual LED appearance
