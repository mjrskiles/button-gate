# FDP-006: Status Word and Bitmask Consolidation

## Status

Proposed

## Summary

Consolidate multiple boolean fields in structs into status words (uint8_t bitmasks) to reduce RAM usage, enable atomic flag operations, and follow embedded C best practices. This refactoring targets the Button, CVOutput, and EventProcessor structs, yielding approximately 15 bytes of RAM savings (~3% of ATtiny85's 512-byte SRAM).

## Motivation

The ATtiny85 has only 512 bytes of SRAM. As Gatekeeper grows in functionality (FSM, menu system, CV processing, LED animations), every byte matters. The current codebase uses individual `bool` fields for state tracking, which is readable but wasteful:

**Current RAM usage for booleans:**

| Struct | Bool Fields | Bytes Used |
|--------|-------------|------------|
| Button | 7 | 7 bytes |
| CVOutput | 3 | 3 bytes |
| IOController | 1 | 1 byte |
| EventProcessor (proposed) | 8 | 8 bytes |
| **Total** | **19** | **19 bytes** |

**With bitmask consolidation:**

| Struct | Status Word | Bytes Used |
|--------|-------------|------------|
| Button | 1 × uint8_t | 1 byte |
| CVOutput | 1 × uint8_t | 1 byte |
| IOController | (merged into CVOutput or controller status) | 0 bytes |
| EventProcessor | 1 × uint8_t | 1 byte |
| **Total** | **3** | **3 bytes** |

**Savings: 16 bytes (~3% of SRAM)**

Beyond memory savings, bitmasks provide:

1. **Atomic operations**: Set/clear multiple flags in one instruction
2. **Efficient comparisons**: Check multiple conditions with single AND
3. **Embedded idiom**: Industry-standard pattern for microcontroller code
4. **Self-documenting**: Named bit definitions clarify intent

## Detailed Design

### Overview

Each struct with multiple boolean fields gets a `status` or `flags` field of type `uint8_t`. Bit positions are defined as named constants. Helper macros provide a clean API for flag manipulation.

### Common Macros

```c
/**
 * Bitmask manipulation macros
 *
 * These provide a clean, consistent API for working with status words.
 * Using macros instead of inline functions avoids call overhead.
 */

// Set bit(s) in status word
#define STATUS_SET(status, mask)    ((status) |= (mask))

// Clear bit(s) in status word
#define STATUS_CLR(status, mask)    ((status) &= ~(mask))

// Toggle bit(s) in status word
#define STATUS_TGL(status, mask)    ((status) ^= (mask))

// Check if bit(s) are set (any of mask)
#define STATUS_ANY(status, mask)    (((status) & (mask)) != 0)

// Check if all bit(s) are set
#define STATUS_ALL(status, mask)    (((status) & (mask)) == (mask))

// Check if bit(s) are clear
#define STATUS_NONE(status, mask)   (((status) & (mask)) == 0)

// Set bit(s) to specific value (0 or 1)
#define STATUS_PUT(status, mask, val) \
    ((val) ? STATUS_SET(status, mask) : STATUS_CLR(status, mask))
```

### Button Module

**Current struct (button.h):**
```c
typedef struct Button {
    uint8_t pin;
    bool raw_state;
    bool pressed;
    bool last_state;
    bool rising_edge;
    bool falling_edge;
    bool config_action;
    uint8_t tap_count;
    uint32_t last_rise_time;
    uint32_t last_fall_time;
    uint32_t last_tap_time;
    bool counting_hold;
} Button;
// Size: 1 + 7 + 1 + 12 = 21 bytes
```

**Proposed struct:**
```c
/**
 * Button status flags
 */
#define BTN_RAW          (1 << 0)  // Raw pin state (before debounce)
#define BTN_PRESSED      (1 << 1)  // Debounced pressed state
#define BTN_LAST         (1 << 2)  // Previous pressed state
#define BTN_RISE         (1 << 3)  // Rising edge detected this cycle
#define BTN_FALL         (1 << 4)  // Falling edge detected this cycle
#define BTN_CONFIG       (1 << 5)  // Config action triggered
#define BTN_COUNTING     (1 << 6)  // Counting hold time for config
// Bit 7 reserved

typedef struct Button {
    uint8_t pin;            // Pin number
    uint8_t status;         // Status flags (see BTN_* defines)
    uint8_t tap_count;      // Tap count for config action
    uint32_t last_rise_time;
    uint32_t last_fall_time;
    uint32_t last_tap_time;
} Button;
// Size: 1 + 1 + 1 + 12 = 15 bytes (was 21, saved 6 bytes)
```

**Usage example:**
```c
void button_update(Button *button) {
    // Read raw state
    STATUS_PUT(button->status, BTN_RAW, p_hal->read_pin(button->pin));

    // Clear edge flags at start of cycle
    STATUS_CLR(button->status, BTN_RISE | BTN_FALL);

    // Detect rising edge
    if (STATUS_ANY(button->status, BTN_RAW) &&
        STATUS_NONE(button->status, BTN_LAST)) {
        STATUS_SET(button->status, BTN_RISE | BTN_PRESSED);
    }

    // Detect falling edge
    if (STATUS_NONE(button->status, BTN_RAW) &&
        STATUS_ANY(button->status, BTN_LAST)) {
        STATUS_SET(button->status, BTN_FALL);
        STATUS_CLR(button->status, BTN_PRESSED);
    }

    // Update last state
    STATUS_PUT(button->status, BTN_LAST, STATUS_ANY(button->status, BTN_PRESSED));
}

// Check for pressed state
if (STATUS_ANY(button->status, BTN_PRESSED)) {
    // Button is pressed
}

// Check for rising edge
if (STATUS_ANY(button->status, BTN_RISE)) {
    // Rising edge this cycle
}
```

### CVOutput Module

**Current struct (cv_output.h):**
```c
typedef struct CVOutput {
    uint8_t pin;
    bool state;
    uint32_t pulse_start_time;
    bool pulse_active;
    bool last_input_state;
} CVOutput;
// Size: 1 + 3 + 4 = 8 bytes
```

**Proposed struct:**
```c
/**
 * CV output status flags
 */
#define CVOUT_STATE      (1 << 0)  // Current output state (high/low)
#define CVOUT_PULSE      (1 << 1)  // Pulse currently active
#define CVOUT_LAST_IN    (1 << 2)  // Last input state (for edge detect)
// Bits 3-7 reserved for future use

typedef struct CVOutput {
    uint8_t pin;            // Output pin number
    uint8_t status;         // Status flags (see CVOUT_* defines)
    uint32_t pulse_start;   // Pulse start timestamp
} CVOutput;
// Size: 1 + 1 + 4 = 6 bytes (was 8, saved 2 bytes)
```

**Usage example:**
```c
bool cv_output_update_pulse(CVOutput *cv, bool input) {
    bool last = STATUS_ANY(cv->status, CVOUT_LAST_IN);

    // Rising edge triggers pulse
    if (input && !last) {
        cv->pulse_start = p_hal->millis();
        STATUS_SET(cv->status, CVOUT_PULSE | CVOUT_STATE);
        p_hal->set_pin(cv->pin);
    }

    // Check pulse expiry
    if (STATUS_ANY(cv->status, CVOUT_PULSE)) {
        if (p_hal->millis() - cv->pulse_start >= PULSE_DURATION_MS) {
            STATUS_CLR(cv->status, CVOUT_PULSE | CVOUT_STATE);
            p_hal->clear_pin(cv->pin);
        }
    }

    STATUS_PUT(cv->status, CVOUT_LAST_IN, input);
    return STATUS_ANY(cv->status, CVOUT_STATE);
}
```

### EventProcessor Module (FDP-004)

**Current proposed struct:**
```c
typedef struct {
    bool a_pressed;
    bool a_was_pressed;
    uint32_t a_press_time;
    bool a_hold_fired;
    bool b_pressed;
    bool b_was_pressed;
    uint32_t b_press_time;
    bool b_hold_fired;
    bool cv_state;
    bool cv_last_state;
    Event pending_event;
} EventProcessor;
// Size: 8 + 8 + 1 = 17 bytes
```

**Proposed struct with bitmask:**
```c
/**
 * Event processor status flags
 *
 * Tracks current and previous state of inputs for edge detection.
 */
#define EVT_A_PRESSED    (1 << 0)  // Button A currently pressed
#define EVT_A_LAST       (1 << 1)  // Button A was pressed last cycle
#define EVT_A_HOLD       (1 << 2)  // Button A hold threshold reached
#define EVT_B_PRESSED    (1 << 3)  // Button B currently pressed
#define EVT_B_LAST       (1 << 4)  // Button B was pressed last cycle
#define EVT_B_HOLD       (1 << 5)  // Button B hold threshold reached
#define EVT_CV_STATE     (1 << 6)  // CV input current state
#define EVT_CV_LAST      (1 << 7)  // CV input last state

typedef struct {
    uint8_t status;             // Input states (see EVT_* defines)
    uint32_t a_press_time;      // Button A press timestamp
    uint32_t b_press_time;      // Button B press timestamp
    Event pending_event;        // Current event (1 byte enum)
} EventProcessor;
// Size: 1 + 8 + 1 = 10 bytes (was 17, saved 7 bytes)
```

**Usage example:**
```c
Event event_processor_update(EventProcessor *ep) {
    uint32_t now = p_hal->millis();
    ep->pending_event = EVT_NONE;

    // Read current states
    STATUS_PUT(ep->status, EVT_A_PRESSED, p_hal->read_pin(p_hal->button_a_pin));
    STATUS_PUT(ep->status, EVT_B_PRESSED, p_hal->read_pin(p_hal->button_b_pin));
    STATUS_PUT(ep->status, EVT_CV_STATE, cv_input_read());

    // Detect Button A rising edge
    if (STATUS_ANY(ep->status, EVT_A_PRESSED) &&
        STATUS_NONE(ep->status, EVT_A_LAST)) {
        ep->a_press_time = now;
        STATUS_CLR(ep->status, EVT_A_HOLD);
        ep->pending_event = EVT_A_PRESS;
    }

    // Detect Button A hold
    if (STATUS_ANY(ep->status, EVT_A_PRESSED) &&
        STATUS_NONE(ep->status, EVT_A_HOLD)) {
        if (now - ep->a_press_time >= HOLD_THRESHOLD_MS) {
            STATUS_SET(ep->status, EVT_A_HOLD);
            ep->pending_event = EVT_A_HOLD;
        }
    }

    // Compound gesture: Menu enter (A:hold + B:tap)
    if (ep->pending_event == EVT_B_TAP &&
        STATUS_ALL(ep->status, EVT_A_PRESSED | EVT_A_HOLD)) {
        ep->pending_event = EVT_MENU_ENTER;
    }

    // Update last states
    STATUS_PUT(ep->status, EVT_A_LAST, STATUS_ANY(ep->status, EVT_A_PRESSED));
    STATUS_PUT(ep->status, EVT_B_LAST, STATUS_ANY(ep->status, EVT_B_PRESSED));
    STATUS_PUT(ep->status, EVT_CV_LAST, STATUS_ANY(ep->status, EVT_CV_STATE));

    return ep->pending_event;
}
```

### CVInput Module (FDP-004)

**Current proposed struct:**
```c
typedef struct {
    uint16_t threshold;
    uint16_t hysteresis;
    bool state;
    bool last_state;
} CVInput;
// Size: 4 + 2 = 6 bytes
```

**Proposed struct with status nibble:**
```c
/**
 * CV input status flags (lower nibble of config byte)
 */
#define CVIN_STATE       (1 << 0)  // Current logical state
#define CVIN_LAST        (1 << 1)  // Previous state
// Bits 2-3 reserved

/**
 * CV input config flags (upper nibble of config byte)
 */
#define CVIN_ENABLED     (1 << 4)  // CV input enabled
#define CVIN_INVERTED    (1 << 5)  // Invert CV logic
// Bits 6-7 reserved

typedef struct {
    uint16_t threshold;     // ADC threshold (0-1023)
    uint16_t hysteresis;    // Hysteresis band
    uint8_t flags;          // Status + config flags
} CVInput;
// Size: 4 + 1 = 5 bytes (was 6, saved 1 byte, gained config flags)
```

### FSM Module (FDP-004)

**Current proposed struct:**
```c
typedef struct FSM {
    const State *states;
    const Transition *transitions;
    uint8_t num_states;
    uint8_t num_transitions;
    uint8_t current_state;
    uint8_t initial_state;
    bool active;
} FSM;
// Size: 4 + 4 + 4 + 1 = 13 bytes (with padding)
```

**Proposed struct:**
```c
/**
 * FSM flags (packed with current_state)
 *
 * Upper 2 bits of state byte hold flags, lower 6 bits hold state ID.
 * Supports up to 64 states per FSM (plenty for our use).
 */
#define FSM_ACTIVE       (1 << 7)  // FSM is active (processing events)
#define FSM_CHANGED      (1 << 6)  // State changed this cycle
#define FSM_STATE_MASK   0x3F      // Lower 6 bits = state ID

typedef struct FSM {
    const State *states;           // PROGMEM pointer
    const Transition *transitions; // PROGMEM pointer
    uint8_t num_states;
    uint8_t num_transitions;
    uint8_t state;                 // Current state + flags (FSM_*)
    uint8_t initial_state;
} FSM;
// Size: 4 + 4 + 4 = 12 bytes (same, but gained CHANGED flag)

// Helper macros
#define FSM_GET_STATE(fsm)     ((fsm)->state & FSM_STATE_MASK)
#define FSM_IS_ACTIVE(fsm)     ((fsm)->state & FSM_ACTIVE)
#define FSM_SET_ACTIVE(fsm)    ((fsm)->state |= FSM_ACTIVE)
#define FSM_CLR_ACTIVE(fsm)    ((fsm)->state &= ~FSM_ACTIVE)
```

### LED Animation (FDP-005)

**Current proposed state:**
```c
static struct {
    LEDState state;
    Color base_color;
    uint32_t last_update;
    uint8_t phase;
    bool current_on;
} led_y_animation;
```

**Proposed with combined state/flags:**
```c
/**
 * LED animation flags (combined with state enum)
 *
 * Lower 2 bits = LEDState enum (0-3)
 * Upper bits = animation flags
 */
#define LED_STATE_MASK   0x03      // Lower 2 bits = state
#define LED_ON_PHASE     (1 << 2)  // Currently in "on" phase of blink
#define LED_DIRTY        (1 << 3)  // Color changed, needs flush

typedef struct {
    uint8_t flags;          // State + flags
    Color base_color;       // Base color for animations
    uint32_t last_update;   // Last animation update time
    uint8_t phase;          // Animation phase (0-255)
} LEDAnimation;
// Combines state enum and bool into single byte
```

### Memory Budget Summary

| Module | Before | After | Saved |
|--------|--------|-------|-------|
| Button | 21 bytes | 15 bytes | 6 bytes |
| CVOutput | 8 bytes | 6 bytes | 2 bytes |
| EventProcessor | 17 bytes | 10 bytes | 7 bytes |
| CVInput | 6 bytes | 5 bytes | 1 byte |
| FSM (×3) | 13 bytes | 12 bytes | 1 byte |
| LEDAnimation | 9 bytes | 8 bytes | 1 byte |
| **Total** | **74 bytes** | **56 bytes** | **18 bytes** |

**18 bytes saved = 3.5% of SRAM**

### Guidelines for Future Code

1. **When to use bitmasks:**
   - 3+ boolean fields in a struct
   - Flags that are frequently tested together
   - Status fields where atomic updates matter

2. **When to keep individual bools:**
   - Single boolean in isolation
   - Bool is frequently passed to/from functions
   - Readability is paramount and RAM isn't critical

3. **Naming conventions:**
   - Status words: `status`, `flags`, or `state`
   - Bit defines: `MODULE_FLAGNAME` (e.g., `BTN_PRESSED`)
   - All caps for bit defines

4. **Documentation:**
   - Comment each bit's meaning
   - Document which bits are reserved
   - Note any special interactions between bits

### Error Handling

| Condition | Handling |
|-----------|----------|
| Reserved bit set | Ignore (allows future expansion) |
| Invalid bit combination | Trust caller (no runtime validation) |
| Status word overflow | N/A (8 bits always sufficient for our use) |

### Testing Strategy

**Unit tests:**

1. **Macro tests**
   - `test_status_set` - Single and multiple bits
   - `test_status_clr` - Single and multiple bits
   - `test_status_tgl` - Toggle behavior
   - `test_status_any` - Partial match
   - `test_status_all` - Full match
   - `test_status_put` - Conditional set/clear

2. **Module integration tests**
   - `test_button_flags` - All button state transitions
   - `test_cvout_flags` - Pulse state machine
   - `test_event_flags` - Event detection with flags

3. **Memory verification**
   - Compile with `-Wpadding` to detect struct padding
   - Use `sizeof()` assertions in tests to verify sizes

## File Changes

| File | Change | Description |
|------|--------|-------------|
| `include/utility/status.h` | Create | Common status macros |
| `include/input/button.h` | Modify | Convert to status word |
| `src/input/button.c` | Modify | Use status macros |
| `include/output/cv_output.h` | Modify | Convert to status word |
| `src/output/cv_output.c` | Modify | Use status macros |
| `include/fsm/events.h` | Modify | Add EventProcessor status flags |
| `src/fsm/events.c` | Modify | Use status macros |
| `include/fsm/fsm.h` | Modify | Pack active flag into state byte |
| `include/input/cv_input.h` | Modify | Add flags byte |
| `include/output/neopixel.h` | Modify | Pack animation flags |
| `test/unit/utility/test_status.h` | Create | Status macro tests |
| Various test files | Modify | Update for new struct layouts |

## Dependencies

- None (pure C, no external libraries)

## Alternatives Considered

### 1. Bit-fields

Using C bit-fields instead of manual masking:

```c
typedef struct {
    uint8_t raw_state : 1;
    uint8_t pressed : 1;
    uint8_t last_state : 1;
    // ...
} ButtonFlags;
```

**Rejected**: Bit-field memory layout is compiler-dependent. Cannot use bitwise operators for bulk operations. Less portable across compilers.

### 2. Keep booleans, optimize elsewhere

Focus RAM optimization on other areas (smaller buffers, etc.).

**Rejected**: Booleans are easy to consolidate with clear benefits. Other optimizations are harder and less impactful.

### 3. Use uint16_t or uint32_t status words

Larger status words for future expansion.

**Rejected**: uint8_t provides 8 flags per struct, which is sufficient. Larger words waste RAM and require more instructions to manipulate on 8-bit AVR.

### 4. Enum flags with named values

```c
typedef enum {
    BTN_RAW = 0x01,
    BTN_PRESSED = 0x02,
    // ...
} ButtonFlag;
```

**Rejected**: Enums may be larger than uint8_t depending on compiler settings. Our `#define` approach with `-fshort-enums` is safer.

## Open Questions

1. **Accessor functions vs macros**: Should we provide inline accessor functions for better type safety, or keep macros for zero overhead?

2. **Atomic operations**: Should status updates be wrapped in cli()/sei() for ISR safety? Currently no ISRs modify these structs.

3. **Debug builds**: Should we add runtime assertions in debug builds to catch invalid flag combinations?

## Implementation Checklist

- [ ] Create `include/utility/status.h` with common macros
- [ ] Refactor Button struct and implementation
- [ ] Refactor CVOutput struct and implementation
- [ ] Update FDP-004 EventProcessor design
- [ ] Update FDP-004 FSM design
- [ ] Update FDP-004 CVInput design
- [ ] Update FDP-005 LED animation design
- [ ] Write status macro unit tests
- [ ] Update existing unit tests for new struct layouts
- [ ] Verify struct sizes with sizeof assertions
- [ ] Update ARCHITECTURE.md with bitmask conventions
