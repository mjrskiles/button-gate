# FDP-005: Neopixel Driver & LED Feedback System

## Status

Proposed

## Summary

Implement a WS2812B (Neopixel) driver for the ATtiny85 and define the LED feedback system for Gatekeeper. Two Neopixels provide visual feedback: LED X indicates the current page/mode via color, LED Y indicates the selected value via state (off, on, blinking, glowing). This replaces the binary LED encoding from the original design with rich, intuitive color feedback.

## Motivation

The rev2 hardware replaces three discrete LEDs with two WS2812B addressable RGB LEDs. This provides:

1. **Richer feedback**: Color encodes more information than on/off
2. **Fewer GPIOs**: Single data line (PB0) vs three GPIO pins
3. **Intuitive UX**: Color-coded pages are easier to learn than binary patterns
4. **Visual appeal**: RGB effects enhance the module's aesthetic

The challenge is driving WS2812B timing-critical protocol from an 8 MHz ATtiny85 while maintaining the main loop responsiveness required for musical applications.

## Detailed Design

### Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    LED Feedback System                       │
└─────────────────────────────────────────────────────────────┘
                              │
           ┌──────────────────┴──────────────────┐
           ▼                                     ▼
    ┌─────────────┐                       ┌─────────────┐
    │   LED X     │                       │   LED Y     │
    │ (Page/Mode) │                       │  (Value)    │
    │             │                       │             │
    │ Color =     │                       │ State =     │
    │ which page  │                       │ which value │
    │ or mode     │                       │ selected    │
    └─────────────┘                       └─────────────┘
           │                                     │
           │         ┌───────────────┐           │
           └────────►│  Neopixel     │◄──────────┘
                     │  Driver       │
                     │  (PB0)        │
                     └───────────────┘
                            │
                            ▼
                     ┌─────────────┐
                     │ WS2812B x2  │
                     │ (daisy-     │
                     │  chained)   │
                     └─────────────┘
```

### WS2812B Protocol

The WS2812B uses a single-wire protocol with strict timing:

| Symbol | Duration | Tolerance |
|--------|----------|-----------|
| T0H (0 bit high) | 400ns | ±150ns |
| T0L (0 bit low) | 850ns | ±150ns |
| T1H (1 bit high) | 800ns | ±150ns |
| T1L (1 bit low) | 450ns | ±150ns |
| Reset | >50µs | - |

At 8 MHz, one cycle = 125ns. Achievable timing:

| Symbol | Target | Cycles | Actual |
|--------|--------|--------|--------|
| T0H | 400ns | 3 | 375ns ✓ |
| T0L | 850ns | 7 | 875ns ✓ |
| T1H | 800ns | 6 | 750ns ✓ |
| T1L | 450ns | 4 | 500ns ✓ |

All timings fall within the ±150ns tolerance window.

### Interface Changes

**Neopixel driver** (`include/output/neopixel.h`):

```c
#ifndef GK_NEOPIXEL_H
#define GK_NEOPIXEL_H

#include <stdint.h>
#include <stdbool.h>

/**
 * RGB color structure
 *
 * WS2812B expects GRB order, but we store as RGB for clarity.
 * Driver handles reordering during transmission.
 */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Color;

/**
 * LED state for value indication (LED Y)
 */
typedef enum {
    LED_STATE_OFF = 0,      // Value 0: LED off
    LED_STATE_ON,           // Value 1: LED solid on
    LED_STATE_BLINK,        // Value 2: LED blinking (~2 Hz)
    LED_STATE_GLOW,         // Value 3: LED pulsing/breathing
} LEDState;

/**
 * Number of LEDs in chain
 */
#define NEOPIXEL_COUNT 2

/**
 * LED indices
 */
#define LED_X 0     // Page/mode indicator
#define LED_Y 1     // Value/selection indicator

/**
 * Initialize Neopixel driver.
 *
 * Configures PB0 as output and sends reset signal.
 */
void neopixel_init(void);

/**
 * Set LED color directly.
 *
 * @param index LED index (LED_X or LED_Y)
 * @param color RGB color to set
 */
void neopixel_set_color(uint8_t index, Color color);

/**
 * Set LED Y state (for value indication).
 *
 * @param state LED state (off, on, blink, glow)
 * @param color Base color for on/blink/glow states
 */
void neopixel_set_state(LEDState state, Color color);

/**
 * Update LED outputs.
 *
 * Must be called periodically to handle blink/glow animations.
 * Transmits data to LEDs if colors have changed.
 *
 * @return true if data was transmitted
 */
bool neopixel_update(void);

/**
 * Force immediate LED update.
 *
 * Transmits current colors to LEDs regardless of change state.
 */
void neopixel_flush(void);

/**
 * Turn off all LEDs.
 */
void neopixel_clear(void);

#endif /* GK_NEOPIXEL_H */
```

**Color definitions** (`include/output/led_colors.h`):

```c
#ifndef GK_LED_COLORS_H
#define GK_LED_COLORS_H

#include "output/neopixel.h"

/**
 * Predefined colors for Gatekeeper UI
 *
 * Colors chosen for:
 * - Distinctiveness (easily distinguishable)
 * - Brightness balance (similar perceived intensity)
 * - Color-blind accessibility where possible
 */

// === Mode Colors (LED X in Perform state) ===
#define COLOR_MODE_GATE     (Color){  0, 255,   0}  // Green
#define COLOR_MODE_TRIGGER  (Color){255, 128,   0}  // Orange
#define COLOR_MODE_TOGGLE   (Color){  0, 128, 255}  // Cyan
#define COLOR_MODE_DIVIDE   (Color){255,   0, 255}  // Magenta
#define COLOR_MODE_CYCLE    (Color){255, 255,   0}  // Yellow

// === Page Colors (LED X in Menu state) ===
#define COLOR_PAGE_GATE_MODE        (Color){  0, 255,   0}  // Green (matches mode)
#define COLOR_PAGE_TRIGGER_BEHAV    (Color){255, 128,   0}  // Orange
#define COLOR_PAGE_TRIGGER_PULSE    (Color){255,  64,   0}  // Dark orange
#define COLOR_PAGE_TOGGLE_MODE      (Color){  0, 128, 255}  // Cyan
#define COLOR_PAGE_DIVIDER_DIVISOR  (Color){255,   0, 255}  // Magenta
#define COLOR_PAGE_CYCLE_BEHAVIOR   (Color){255, 255,   0}  // Yellow
#define COLOR_PAGE_CV_GLOBAL        (Color){255, 255, 255}  // White
#define COLOR_PAGE_MENU_TIMEOUT     (Color){128, 128, 128}  // Gray

// === Value indication (LED Y base color) ===
#define COLOR_VALUE_DEFAULT (Color){255, 255, 255}  // White

// === Special states ===
#define COLOR_OFF           (Color){  0,   0,   0}  // Off
#define COLOR_ERROR         (Color){255,   0,   0}  // Red (error/warning)
#define COLOR_FACTORY_RESET (Color){255,   0,   0}  // Red (during reset)

// === Brightness levels ===
#define BRIGHTNESS_FULL     255
#define BRIGHTNESS_DIM      64
#define BRIGHTNESS_GLOW_MIN 16
#define BRIGHTNESS_GLOW_MAX 200

/**
 * Mode to color lookup (PROGMEM)
 */
extern const Color mode_colors[] PROGMEM;

/**
 * Page to color lookup (PROGMEM)
 */
extern const Color page_colors[] PROGMEM;

/**
 * Get color for mode.
 */
Color led_get_mode_color(uint8_t mode);

/**
 * Get color for menu page.
 */
Color led_get_page_color(uint8_t page);

/**
 * Apply brightness scaling to color.
 */
Color led_scale_brightness(Color color, uint8_t brightness);

#endif /* GK_LED_COLORS_H */
```

**LED feedback controller** (`include/output/led_feedback.h`):

```c
#ifndef GK_LED_FEEDBACK_H
#define GK_LED_FEEDBACK_H

#include <stdint.h>
#include "output/neopixel.h"

/**
 * LED Feedback Controller
 *
 * High-level interface for FSM to control LED feedback.
 * Abstracts Neopixel details and handles mode/menu state display.
 */

/**
 * Initialize LED feedback system.
 */
void led_feedback_init(void);

/**
 * Set LED X to show current mode (Perform state).
 *
 * @param mode Current mode (MODE_GATE, MODE_TRIGGER, etc.)
 */
void led_feedback_show_mode(uint8_t mode);

/**
 * Set LED X to show current menu page (Menu state).
 *
 * @param page Current page (PAGE_GATE_MODE, etc.)
 */
void led_feedback_show_page(uint8_t page);

/**
 * Set LED Y to show current selection value.
 *
 * @param value Selection value (0-3 maps to off/on/blink/glow)
 */
void led_feedback_show_value(uint8_t value);

/**
 * Set LED Y to show output state (Perform state).
 *
 * @param active true if output is high, false if low
 */
void led_feedback_show_output(bool active);

/**
 * Flash error pattern (e.g., for EEPROM validation failure).
 */
void led_feedback_show_error(void);

/**
 * Show factory reset progress.
 *
 * @param progress 0-100 percentage
 */
void led_feedback_show_reset_progress(uint8_t progress);

/**
 * Update LED feedback.
 *
 * Call every main loop iteration to handle animations.
 */
void led_feedback_update(void);

#endif /* GK_LED_FEEDBACK_H */
```

### Implementation Details

#### 1. Bit-Banged WS2812B Driver

The driver uses inline assembly for cycle-accurate timing:

```c
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define NEOPIXEL_PORT PORTB
#define NEOPIXEL_DDR  DDRB
#define NEOPIXEL_PIN  PB0

// LED data buffer (GRB order for transmission)
static uint8_t led_buffer[NEOPIXEL_COUNT * 3];

/**
 * Send a single byte to the Neopixel chain.
 *
 * Timing-critical: interrupts must be disabled.
 * At 8 MHz: 1 cycle = 125ns
 */
static void neopixel_send_byte(uint8_t byte) {
    for (uint8_t bit = 0; bit < 8; bit++) {
        if (byte & 0x80) {
            // Send 1: T1H=750ns (6 cycles), T1L=500ns (4 cycles)
            asm volatile(
                "sbi %[port], %[pin]"   "\n\t"  // 2 cycles - high
                "nop" "\n\t"                     // 1 cycle
                "nop" "\n\t"                     // 1 cycle
                "nop" "\n\t"                     // 1 cycle
                "nop" "\n\t"                     // 1 cycle  = 6 cycles high
                "cbi %[port], %[pin]"   "\n\t"  // 2 cycles - low
                "nop" "\n\t"                     // 1 cycle
                "nop" "\n\t"                     // 1 cycle  = 4 cycles low
                :
                : [port] "I" (_SFR_IO_ADDR(NEOPIXEL_PORT)),
                  [pin] "I" (NEOPIXEL_PIN)
            );
        } else {
            // Send 0: T0H=375ns (3 cycles), T0L=875ns (7 cycles)
            asm volatile(
                "sbi %[port], %[pin]"   "\n\t"  // 2 cycles - high
                "nop" "\n\t"                     // 1 cycle  = 3 cycles high
                "cbi %[port], %[pin]"   "\n\t"  // 2 cycles - low
                "nop" "\n\t"                     // 1 cycle
                "nop" "\n\t"                     // 1 cycle
                "nop" "\n\t"                     // 1 cycle
                "nop" "\n\t"                     // 1 cycle
                "nop" "\n\t"                     // 1 cycle  = 7 cycles low
                :
                : [port] "I" (_SFR_IO_ADDR(NEOPIXEL_PORT)),
                  [pin] "I" (NEOPIXEL_PIN)
            );
        }
        byte <<= 1;
    }
}

/**
 * Transmit all LED data.
 *
 * Disables interrupts during transmission (~60µs for 2 LEDs).
 */
void neopixel_flush(void) {
    cli();  // Disable interrupts

    for (uint8_t i = 0; i < NEOPIXEL_COUNT * 3; i++) {
        neopixel_send_byte(led_buffer[i]);
    }

    sei();  // Re-enable interrupts

    _delay_us(50);  // Reset pulse
}
```

**Interrupt latency consideration:**

Transmission of 2 LEDs = 48 bits = ~60µs with interrupts disabled. At 1ms timer tick, this is acceptable. The timer ISR will fire immediately after transmission completes, causing at most 60µs jitter in `millis()` - negligible for our timing requirements.

#### 2. Animation Engine

Blink and glow effects are handled in `neopixel_update()`:

```c
// Animation state
static struct {
    LEDState state;
    Color base_color;
    uint32_t last_update;
    uint8_t phase;          // Animation phase (0-255)
    bool current_on;        // For blink: current on/off state
} led_y_animation;

#define BLINK_PERIOD_MS     500     // 2 Hz blink
#define GLOW_PERIOD_MS      2000    // 0.5 Hz breathing

bool neopixel_update(void) {
    uint32_t now = p_hal->millis();
    bool changed = false;

    switch (led_y_animation.state) {
        case LED_STATE_OFF:
            // Static - no animation
            break;

        case LED_STATE_ON:
            // Static - no animation
            break;

        case LED_STATE_BLINK:
            if (now - led_y_animation.last_update >= BLINK_PERIOD_MS / 2) {
                led_y_animation.last_update = now;
                led_y_animation.current_on = !led_y_animation.current_on;

                if (led_y_animation.current_on) {
                    led_buffer[LED_Y * 3 + 0] = led_y_animation.base_color.g;
                    led_buffer[LED_Y * 3 + 1] = led_y_animation.base_color.r;
                    led_buffer[LED_Y * 3 + 2] = led_y_animation.base_color.b;
                } else {
                    led_buffer[LED_Y * 3 + 0] = 0;
                    led_buffer[LED_Y * 3 + 1] = 0;
                    led_buffer[LED_Y * 3 + 2] = 0;
                }
                changed = true;
            }
            break;

        case LED_STATE_GLOW: {
            // Update every ~8ms for smooth 256-step animation
            if (now - led_y_animation.last_update >= GLOW_PERIOD_MS / 256) {
                led_y_animation.last_update = now;
                led_y_animation.phase++;

                // Triangle wave: 0→255→0
                uint8_t brightness;
                if (led_y_animation.phase < 128) {
                    brightness = led_y_animation.phase * 2;
                } else {
                    brightness = (255 - led_y_animation.phase) * 2;
                }

                // Scale to GLOW_MIN..GLOW_MAX range
                brightness = BRIGHTNESS_GLOW_MIN +
                    (brightness * (BRIGHTNESS_GLOW_MAX - BRIGHTNESS_GLOW_MIN)) / 255;

                Color scaled = led_scale_brightness(led_y_animation.base_color, brightness);
                led_buffer[LED_Y * 3 + 0] = scaled.g;
                led_buffer[LED_Y * 3 + 1] = scaled.r;
                led_buffer[LED_Y * 3 + 2] = scaled.b;
                changed = true;
            }
            break;
        }
    }

    if (changed) {
        neopixel_flush();
    }

    return changed;
}
```

#### 3. FSM Integration

The LED feedback controller is called from FSM entry/exit actions and the main update loop:

```c
// In gatekeeper_fsm.c

static void on_perform_enter(void) {
    uint8_t mode = fsm_get_state(&gk_fsm.mode_fsm);
    led_feedback_show_mode(mode);
}

static void on_menu_enter(void) {
    uint8_t page = fsm_get_state(&gk_fsm.menu_fsm);
    led_feedback_show_page(page);
    led_feedback_show_value(get_current_page_value());
}

static void action_next_page(void) {
    uint8_t page = (fsm_get_state(&gk_fsm.menu_fsm) + 1) % PAGE_COUNT;
    fsm_set_state(&gk_fsm.menu_fsm, page);
    led_feedback_show_page(page);
    led_feedback_show_value(get_current_page_value());
}

static void action_next_selection(void) {
    uint8_t page = fsm_get_state(&gk_fsm.menu_fsm);
    uint8_t value = cycle_page_value(page);  // Increment and wrap
    led_feedback_show_value(value);
}

// In main loop
void gatekeeper_fsm_update(GatekeeperFSM *gk) {
    // ... event processing ...

    // Update LED animations
    led_feedback_update();

    // Update output indicator (LED Y shows output state in Perform mode)
    if (fsm_get_state(&gk->top_fsm) == TOP_PERFORM) {
        led_feedback_show_output(gk->cv_output_state);
    }
}
```

### Data Structures

**Color lookup tables (PROGMEM):**

```c
const Color mode_colors[] PROGMEM = {
    {  0, 255,   0},    // MODE_GATE - Green
    {255, 128,   0},    // MODE_TRIGGER - Orange
    {  0, 128, 255},    // MODE_TOGGLE - Cyan
    {255,   0, 255},    // MODE_DIVIDE - Magenta
    {255, 255,   0},    // MODE_CYCLE - Yellow
};

const Color page_colors[] PROGMEM = {
    {  0, 255,   0},    // PAGE_GATE_MODE - Green
    {255, 128,   0},    // PAGE_TRIGGER_BEHAVIOR - Orange
    {255,  64,   0},    // PAGE_TRIGGER_PULSE_LEN - Dark orange
    {  0, 128, 255},    // PAGE_TOGGLE_MODE - Cyan
    {255,   0, 255},    // PAGE_DIVIDER_DIVISOR - Magenta
    {255, 255,   0},    // PAGE_CYCLE_BEHAVIOR - Yellow
    {255, 255, 255},    // PAGE_CV_GLOBAL - White
    {128, 128, 128},    // PAGE_MENU_TIMEOUT - Gray
};
```

**Memory budget:**

| Component | RAM | PROGMEM |
|-----------|-----|---------|
| LED buffer (2 LEDs × 3 bytes) | 6 bytes | - |
| Animation state | 10 bytes | - |
| Color lookup tables | - | ~40 bytes |
| Driver code | - | ~200 bytes |
| **Total** | ~16 bytes | ~240 bytes |

### Error Handling

| Condition | Handling |
|-----------|----------|
| Invalid LED index | Clamp to valid range |
| Invalid color values | Accept as-is (0-255 all valid) |
| Interrupt during transmit | Prevented by cli()/sei() |
| Timer drift during animation | Self-correcting (uses absolute time) |

### Testing Strategy

**Unit tests** (mock HAL):

1. **Neopixel driver tests**
   - `test_neopixel_init` - Pin configured as output
   - `test_neopixel_set_color` - Buffer updated correctly
   - `test_neopixel_grb_order` - RGB→GRB conversion correct

2. **Animation tests**
   - `test_led_state_off` - LED stays off
   - `test_led_state_on` - LED stays on
   - `test_led_state_blink` - Toggles at correct frequency
   - `test_led_state_glow` - Brightness cycles smoothly

3. **Color tests**
   - `test_mode_color_lookup` - Correct colors for each mode
   - `test_page_color_lookup` - Correct colors for each page
   - `test_brightness_scaling` - Scales RGB proportionally

4. **Integration tests**
   - `test_mode_change_updates_led` - LED X changes on mode change
   - `test_menu_page_updates_led` - LED X changes on page change
   - `test_menu_value_updates_led` - LED Y changes on value change

**Hardware tests:**
- Visual inspection of all colors
- Timing verification with logic analyzer
- Animation smoothness check
- Verify no visible flicker during updates

## File Changes

| File | Change | Description |
|------|--------|-------------|
| `include/output/neopixel.h` | Create | Neopixel driver interface |
| `include/output/led_colors.h` | Create | Color definitions and lookups |
| `include/output/led_feedback.h` | Create | High-level LED feedback controller |
| `src/output/neopixel.c` | Create | Neopixel driver implementation |
| `src/output/led_colors.c` | Create | Color lookup tables |
| `src/output/led_feedback.c` | Create | LED feedback controller |
| `src/output/CMakeLists.txt` | Modify | Add new source files |
| `include/hardware/hal.h` | Modify | Remove legacy LED pin defines |
| `test/unit/output/test_neopixel.h` | Create | Neopixel driver tests |
| `test/unit/output/test_led_feedback.h` | Create | LED feedback tests |
| `test/unit/CMakeLists.txt` | Modify | Add LED test sources |

## Dependencies

- AVR I/O headers (`<avr/io.h>`)
- AVR interrupt control (`<avr/interrupt.h>`)
- Util delay (`<util/delay.h>`)
- Existing HAL (millis for animation timing)
- FSM module (FDP-004) for state integration

## Alternatives Considered

### 1. light_ws2812 Library

Using the existing [light_ws2812](https://github.com/cpldcpu/light_ws2812) library.

**Considered but rejected**: The library is well-tested but includes features we don't need (multiple platform support, various configurations). A minimal custom driver is smaller and gives us full control over timing.

### 2. Hardware PWM for Animations

Using Timer1 PWM output for LED brightness control.

**Rejected**: WS2812B requires specific protocol, not PWM. Animations must be done in software regardless. Timer1 PWM doesn't help.

### 3. DMA-style Buffer Transmission

Pre-computing the entire bit stream and transmitting via timer interrupt.

**Rejected**: Overkill for 2 LEDs. The ~60µs blocking transmission is acceptable. Would consume significant RAM for bit buffer.

### 4. Separate Animation Task

Running animation updates in timer ISR.

**Rejected**: Adds complexity. Main loop update rate is sufficient for smooth animations. ISR should remain minimal.

## Open Questions

1. **Color tuning**: Final color values may need adjustment based on actual LED appearance. Should we add a calibration mechanism?

2. **Brightness adjustment**: Should overall brightness be user-configurable? Could save power and reduce eye strain in dark environments.

3. **Output indication in Perform**: Should LED Y show output state (gate high/low) or be available for other feedback? Current design shows output state.

4. **Color-blind accessibility**: Current palette relies heavily on color. Should we add pattern/brightness variations for accessibility?

## Implementation Checklist

- [ ] Create Neopixel driver with inline assembly timing
- [ ] Implement color lookup tables in PROGMEM
- [ ] Implement LED feedback controller
- [ ] Implement blink animation
- [ ] Implement glow/breathing animation
- [ ] Write Neopixel driver tests
- [ ] Write animation tests
- [ ] Write color lookup tests
- [ ] Integrate with FSM module
- [ ] Test on hardware with logic analyzer
- [ ] Tune colors for visual appearance
- [ ] Verify timing meets WS2812B spec
- [ ] Profile CPU time for LED updates


# Michael's response

Q's

1. Let's not worry about calibration just yet

2. Let's keep it up our sleeve, but I don't want to bloat the menu just yet till we get our hands on it and see what settings will really easy pain points.

3. I think since we'll have the buffered output tied directly to an LED (let's call it LED Z, the analog circuit one), we can free up LED Y for something else. What are some good suggestions for both X and Y during perform state?

4. Heck yeah! I always like to at least make an effort at accessibility. So lets brainstorm a few colorblind/accessible options.