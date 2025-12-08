# FDP-004: State Machine Module

## Status

Proposed

## Summary

Implement a reusable, table-driven hierarchical finite state machine (FSM) engine for Gatekeeper. The FSM module will manage application state across Boot, Perform, and Menu contexts with separate sub-FSMs for mode selection and menu navigation. Transition tables are stored in PROGMEM to conserve RAM on the ATtiny85.

## Motivation

The current implementation uses ad-hoc switch-case logic in `io_controller.c` for mode handling. As Gatekeeper expands to support:

- 5 operating modes (Gate, Trigger, Toggle, Divide, Cycle)
- A menu system with 7+ settings pages
- Complex button gestures (hold, tap, compound)
- CV input with configurable behavior per mode

...the switch-case approach becomes unwieldy and error-prone. A formal state machine architecture provides:

1. **Predictable behavior**: All states and transitions explicitly defined
2. **Testability**: FSM logic can be unit tested in isolation
3. **Extensibility**: Adding modes/pages requires only table entries, not code changes
4. **Memory efficiency**: PROGMEM storage for static transition tables
5. **Educational value**: Demonstrates embedded FSM patterns for reference project

## Detailed Design

### Overview

The state machine architecture consists of three layers:

```
┌─────────────────────────────────────────────────────────────┐
│                     Event Processor                         │
│  (Button debounce, gesture detection, CV edge detection)    │
└─────────────────────────────────────────────────────────────┘
                              │ Events
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      Top-Level FSM                          │
│              PERFORM  ◄─────────────►  MENU                 │
└─────────────────────────────────────────────────────────────┘
                    │                         │
                    ▼                         ▼
        ┌───────────────────┐     ┌───────────────────┐
        │     Mode FSM      │     │     Menu FSM      │
        │ (active in PERF)  │     │ (active in MENU)  │
        │                   │     │                   │
        │  GATE ─► TRIGGER  │     │  PAGE_GATE_MODE   │
        │    ▲        │     │     │        │          │
        │    │        ▼     │     │        ▼          │
        │ CYCLE ◄─ TOGGLE   │     │  PAGE_TRIG_BEHAV  │
        │    ▲        │     │     │        │          │
        │    │        ▼     │     │       ...         │
        │    └─── DIVIDE    │     │        │          │
        │                   │     │        ▼          │
        └───────────────────┘     │  PAGE_CV_GLOBAL   │
                                  │        │          │
                                  │        ▼          │
                                  │  (wraps to top)   │
                                  └───────────────────┘
```

### Interface Changes

**New public interface** (`include/fsm/fsm.h`):

```c
#ifndef GK_FSM_H
#define GK_FSM_H

#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct FSM FSM;
typedef struct State State;
typedef struct Transition Transition;

/**
 * State definition
 *
 * Each state has optional entry/exit actions and an update function
 * called every main loop iteration while the state is active.
 */
typedef struct State {
    uint8_t id;
    void (*on_enter)(void);     // Called once on state entry (may be NULL)
    void (*on_exit)(void);      // Called once on state exit (may be NULL)
    void (*on_update)(void);    // Called every tick while active (may be NULL)
} State;

/**
 * Transition definition (PROGMEM-compatible)
 *
 * Maps (current_state, event) -> (next_state, action).
 * Use FSM_NO_TRANSITION for next_state to indicate no transition.
 */
typedef struct Transition {
    uint8_t from_state;
    uint8_t event;
    uint8_t to_state;
    void (*action)(void);       // Transition action (may be NULL)
} Transition;

/**
 * FSM instance
 *
 * Holds pointers to state/transition tables (in PROGMEM) and current state.
 */
typedef struct FSM {
    const State *states;              // Pointer to state array (PROGMEM)
    const Transition *transitions;    // Pointer to transition array (PROGMEM)
    uint8_t num_states;
    uint8_t num_transitions;
    uint8_t current_state;
    uint8_t initial_state;
    bool active;                      // For hierarchical FSMs
} FSM;

// Special values
#define FSM_NO_TRANSITION  0xFF
#define FSM_ANY_STATE      0xFE

/**
 * Initialize an FSM instance.
 *
 * @param fsm           Pointer to FSM struct to initialize
 * @param states        Pointer to state array (PROGMEM)
 * @param num_states    Number of states
 * @param transitions   Pointer to transition array (PROGMEM)
 * @param num_trans     Number of transitions
 * @param initial       Initial state ID
 */
void fsm_init(FSM *fsm, const State *states, uint8_t num_states,
              const Transition *transitions, uint8_t num_trans,
              uint8_t initial);

/**
 * Process an event through the FSM.
 *
 * Searches transition table for matching (current_state, event).
 * If found: executes exit action, transition action, updates state,
 * executes entry action.
 *
 * @param fsm   Pointer to FSM instance
 * @param event Event to process
 * @return      true if a transition occurred, false otherwise
 */
bool fsm_process_event(FSM *fsm, uint8_t event);

/**
 * Run the current state's update function.
 *
 * Should be called every main loop iteration.
 *
 * @param fsm   Pointer to FSM instance
 */
void fsm_update(FSM *fsm);

/**
 * Reset FSM to initial state.
 *
 * Calls exit action on current state, then entry action on initial state.
 *
 * @param fsm   Pointer to FSM instance
 */
void fsm_reset(FSM *fsm);

/**
 * Get current state ID.
 *
 * @param fsm   Pointer to FSM instance
 * @return      Current state ID
 */
uint8_t fsm_get_state(const FSM *fsm);

/**
 * Force transition to a specific state.
 *
 * Bypasses transition table. Useful for context-aware jumps
 * (e.g., menu entry at mode-specific page).
 *
 * @param fsm       Pointer to FSM instance
 * @param state_id  Target state ID
 */
void fsm_set_state(FSM *fsm, uint8_t state_id);

#endif /* GK_FSM_H */
```

**Event types** (`include/fsm/events.h`):

```c
#ifndef GK_FSM_EVENTS_H
#define GK_FSM_EVENTS_H

/**
 * Event types for the Gatekeeper FSM
 *
 * Events are categorized by timing:
 * - PRESS events: Fire immediately on button down (performance-critical)
 * - RELEASE events: Fire on button up (configuration actions)
 * - HOLD events: Fire after hold threshold while still pressed
 * - Compound events: Detected from button combinations
 */
typedef enum {
    EVT_NONE = 0,

    // === Performance events (on press, fast response) ===
    EVT_A_PRESS,            // Button A pressed
    EVT_B_PRESS,            // Button B pressed
    EVT_CV_RISE,            // CV input rising edge
    EVT_CV_FALL,            // CV input falling edge

    // === Configuration events (on release, deliberate) ===
    EVT_A_TAP,              // Button A tap (short press + release)
    EVT_A_RELEASE,          // Button A released (any duration)
    EVT_B_TAP,              // Button B tap (short press + release)
    EVT_B_RELEASE,          // Button B released (any duration)

    // === Hold events (threshold reached while held) ===
    EVT_A_HOLD,             // Button A held past threshold
    EVT_B_HOLD,             // Button B held past threshold

    // === Compound gesture events ===
    EVT_MENU_ENTER,         // A:hold + B:tap → enter menu
    EVT_MENU_EXIT,          // A:release while in menu → exit menu
    EVT_MODE_CHANGE,        // B:hold + A:hold → advance mode

    // === Timing events ===
    EVT_TIMEOUT,            // Generic timeout (context-dependent)
    EVT_PULSE_COMPLETE,     // Pulse duration elapsed

    EVT_COUNT               // Number of events (for array sizing)
} Event;

#endif /* GK_FSM_EVENTS_H */
```

### Implementation Details

#### 1. PROGMEM Storage

Transition tables are stored in flash to conserve RAM:

```c
#include <avr/pgmspace.h>

// Mode FSM transitions (in PROGMEM)
const Transition mode_transitions[] PROGMEM = {
    // from_state,      event,           to_state,        action
    { MODE_GATE,        EVT_MODE_CHANGE, MODE_TRIGGER,    NULL },
    { MODE_TRIGGER,     EVT_MODE_CHANGE, MODE_TOGGLE,     NULL },
    { MODE_TOGGLE,      EVT_MODE_CHANGE, MODE_DIVIDE,     NULL },
    { MODE_DIVIDE,      EVT_MODE_CHANGE, MODE_CYCLE,      NULL },
    { MODE_CYCLE,       EVT_MODE_CHANGE, MODE_GATE,       NULL },
};
```

The `fsm_process_event()` function uses `pgm_read_byte()` and `pgm_read_ptr()` to access table entries:

```c
bool fsm_process_event(FSM *fsm, uint8_t event) {
    for (uint8_t i = 0; i < fsm->num_transitions; i++) {
        uint8_t from = pgm_read_byte(&fsm->transitions[i].from_state);
        uint8_t evt = pgm_read_byte(&fsm->transitions[i].event);

        if ((from == fsm->current_state || from == FSM_ANY_STATE) &&
            evt == event) {

            uint8_t to = pgm_read_byte(&fsm->transitions[i].to_state);
            if (to == FSM_NO_TRANSITION) {
                // Event handled but no state change
                void (*action)(void) = pgm_read_ptr(&fsm->transitions[i].action);
                if (action) action();
                return false;
            }

            // Execute transition
            execute_exit_action(fsm);
            void (*action)(void) = pgm_read_ptr(&fsm->transitions[i].action);
            if (action) action();
            fsm->current_state = to;
            execute_entry_action(fsm);
            return true;
        }
    }
    return false;
}
```

#### 2. Event Processor

The event processor transforms raw button/CV states into semantic events:

```c
// Event processor state
typedef struct {
    // Button A tracking
    bool a_pressed;
    bool a_was_pressed;
    uint32_t a_press_time;
    bool a_hold_fired;

    // Button B tracking
    bool b_pressed;
    bool b_was_pressed;
    uint32_t b_press_time;
    bool b_hold_fired;

    // CV tracking
    bool cv_state;
    bool cv_last_state;

    // Pending event (single event per update cycle)
    Event pending_event;
} EventProcessor;

/**
 * Update event processor and return next event.
 *
 * Call once per main loop iteration. Returns EVT_NONE if no event.
 * Press events fire immediately; release/hold events fire on state change.
 */
Event event_processor_update(EventProcessor *ep);
```

**Gesture detection logic:**

```c
Event event_processor_update(EventProcessor *ep) {
    uint32_t now = p_hal->millis();
    ep->pending_event = EVT_NONE;

    // Read current button states
    ep->a_pressed = p_hal->read_pin(p_hal->button_a_pin);
    ep->b_pressed = p_hal->read_pin(p_hal->button_b_pin);

    // === Button A ===
    // Rising edge (press)
    if (ep->a_pressed && !ep->a_was_pressed) {
        ep->a_press_time = now;
        ep->a_hold_fired = false;
        ep->pending_event = EVT_A_PRESS;
    }
    // Falling edge (release)
    else if (!ep->a_pressed && ep->a_was_pressed) {
        // Check for compound gesture: A:hold + B:tap (menu enter)
        // This is detected on B release, not A release

        // Check if this was a tap (short press)
        if ((now - ep->a_press_time) < HOLD_THRESHOLD_MS) {
            ep->pending_event = EVT_A_TAP;
        } else {
            ep->pending_event = EVT_A_RELEASE;
        }
    }
    // Hold detection
    else if (ep->a_pressed && !ep->a_hold_fired) {
        if ((now - ep->a_press_time) >= HOLD_THRESHOLD_MS) {
            ep->a_hold_fired = true;
            ep->pending_event = EVT_A_HOLD;
        }
    }

    // === Button B (similar logic) ===
    // ...

    // === Compound gestures ===
    // Menu enter: A held + B tap (detected on B release)
    if (ep->pending_event == EVT_B_TAP && ep->a_pressed && ep->a_hold_fired) {
        ep->pending_event = EVT_MENU_ENTER;
    }

    // Mode change: Both held (detected when both hold thresholds reached)
    if (ep->a_hold_fired && ep->b_hold_fired) {
        ep->pending_event = EVT_MODE_CHANGE;
        ep->a_hold_fired = false;  // Reset to allow re-trigger
        ep->b_hold_fired = false;
    }

    // === CV Input ===
    ep->cv_state = p_hal->read_pin(p_hal->cv_in_pin);
    if (ep->cv_state && !ep->cv_last_state) {
        ep->pending_event = EVT_CV_RISE;
    } else if (!ep->cv_state && ep->cv_last_state) {
        ep->pending_event = EVT_CV_FALL;
    }
    ep->cv_last_state = ep->cv_state;

    ep->a_was_pressed = ep->a_pressed;
    ep->b_was_pressed = ep->b_pressed;

    return ep->pending_event;
}
```

#### 3. Hierarchical FSM Coordinator

The coordinator manages the top-level FSM and activates/deactivates child FSMs:

```c
typedef struct {
    FSM top_fsm;        // PERFORM ↔ MENU
    FSM mode_fsm;       // Gate, Trigger, Toggle, Divide, Cycle
    FSM menu_fsm;       // Settings pages (flat ring)

    EventProcessor events;
    AppSettings *settings;
} GatekeeperFSM;

/**
 * Top-level states
 */
typedef enum {
    TOP_PERFORM = 0,
    TOP_MENU,
} TopState;

/**
 * Mode states (child of PERFORM)
 */
typedef enum {
    MODE_GATE = 0,
    MODE_TRIGGER,
    MODE_TOGGLE,
    MODE_DIVIDE,
    MODE_CYCLE,
    MODE_COUNT,
} ModeState;

/**
 * Menu page states (child of MENU)
 *
 * Pages form a flat ring - A:tap cycles through all pages.
 * Mode-specific pages are grouped but all accessible from any mode.
 */
typedef enum {
    PAGE_GATE_MODE = 0,
    PAGE_TRIGGER_BEHAVIOR,
    PAGE_TRIGGER_PULSE_LEN,
    PAGE_TOGGLE_MODE,
    PAGE_DIVIDER_DIVISOR,
    PAGE_CYCLE_BEHAVIOR,
    PAGE_CV_GLOBAL,
    PAGE_MENU_TIMEOUT,      // Menu timeout setting (15-30s)
    PAGE_COUNT,
} MenuPage;

/**
 * Map mode to its first settings page (for context-aware menu entry)
 */
static const uint8_t mode_to_page[] PROGMEM = {
    PAGE_GATE_MODE,         // MODE_GATE
    PAGE_TRIGGER_BEHAVIOR,  // MODE_TRIGGER
    PAGE_TOGGLE_MODE,       // MODE_TOGGLE
    PAGE_DIVIDER_DIVISOR,   // MODE_DIVIDE
    PAGE_CYCLE_BEHAVIOR,    // MODE_CYCLE
};
```

#### 4. State Entry/Exit Actions

```c
// Entry action for MENU state
static void on_menu_enter(void) {
    // Jump to page relevant to current mode
    uint8_t current_mode = fsm_get_state(&gk_fsm.mode_fsm);
    uint8_t target_page = pgm_read_byte(&mode_to_page[current_mode]);
    fsm_set_state(&gk_fsm.menu_fsm, target_page);

    // Pause mode FSM
    gk_fsm.mode_fsm.active = false;
    gk_fsm.menu_fsm.active = true;

    // Update LED to show page color
    update_page_led(target_page);
}

// Exit action for MENU state
static void on_menu_exit(void) {
    // Save settings to EEPROM (uses eeprom_update_* for wear protection)
    app_init_save_settings(gk_fsm.settings);

    // Resume mode FSM
    gk_fsm.menu_fsm.active = false;
    gk_fsm.mode_fsm.active = true;

    // Restore mode LED indication
    update_mode_led(fsm_get_state(&gk_fsm.mode_fsm));
}
```

#### 5. Main Loop Integration

```c
void gatekeeper_fsm_update(GatekeeperFSM *gk) {
    // 1. Process inputs and generate event
    Event event = event_processor_update(&gk->events);

    // 2. Route event to top-level FSM
    if (event != EVT_NONE) {
        fsm_process_event(&gk->top_fsm, event);

        // 3. Route to active child FSM
        if (gk->mode_fsm.active) {
            fsm_process_event(&gk->mode_fsm, event);
        }
        if (gk->menu_fsm.active) {
            fsm_process_event(&gk->menu_fsm, event);
        }
    }

    // 4. Run update functions
    fsm_update(&gk->top_fsm);
    if (gk->mode_fsm.active) {
        fsm_update(&gk->mode_fsm);
    }
    if (gk->menu_fsm.active) {
        fsm_update(&gk->menu_fsm);
    }
}
```

### Data Structures

**Top-level FSM transitions:**

```c
const Transition top_transitions[] PROGMEM = {
    { TOP_PERFORM, EVT_MENU_ENTER, TOP_MENU,    NULL },
    { TOP_MENU,    EVT_MENU_EXIT,  TOP_PERFORM, NULL },
};
```

**Mode FSM transitions:**

```c
const Transition mode_transitions[] PROGMEM = {
    { FSM_ANY_STATE, EVT_MODE_CHANGE, FSM_NO_TRANSITION, action_next_mode },
};

static void action_next_mode(void) {
    uint8_t current = fsm_get_state(&gk_fsm.mode_fsm);
    uint8_t next = (current + 1) % MODE_COUNT;
    fsm_set_state(&gk_fsm.mode_fsm, next);
    gk_fsm.settings->mode = next;
}
```

**Menu FSM transitions:**

```c
const Transition menu_transitions[] PROGMEM = {
    // A:tap cycles page (ring navigation)
    { FSM_ANY_STATE, EVT_A_TAP, FSM_NO_TRANSITION, action_next_page },

    // B:tap cycles selection within page
    { FSM_ANY_STATE, EVT_B_TAP, FSM_NO_TRANSITION, action_next_selection },
};
```

**Memory budget estimate:**

| Component | RAM | PROGMEM |
|-----------|-----|---------|
| FSM struct (top) | 8 bytes | - |
| FSM struct (mode) | 8 bytes | - |
| FSM struct (menu) | 8 bytes | - |
| EventProcessor | 20 bytes | - |
| State arrays | - | ~60 bytes |
| Transition tables | - | ~80 bytes |
| **Total** | ~44 bytes | ~140 bytes |

### Error Handling

| Condition | Handling |
|-----------|----------|
| NULL FSM pointer | Return early, no-op |
| Invalid state ID | Clamp to valid range, log if debug enabled |
| No matching transition | Event ignored (normal behavior) |
| NULL action pointer | Skip action, continue transition |

### Testing Strategy

**Unit tests** (mock HAL):

1. **FSM engine tests**
   - `test_fsm_init` - Verify initial state set correctly
   - `test_fsm_transition` - Valid transition executes
   - `test_fsm_no_transition` - Unhandled event returns false
   - `test_fsm_entry_exit_actions` - Actions called in correct order
   - `test_fsm_any_state` - FSM_ANY_STATE matches any current state
   - `test_fsm_reset` - Reset returns to initial state

2. **Event processor tests**
   - `test_event_press_detection` - Press fires on button down
   - `test_event_tap_detection` - Tap fires on short press+release
   - `test_event_hold_detection` - Hold fires after threshold
   - `test_event_compound_menu_enter` - A:hold+B:tap detected
   - `test_event_compound_mode_change` - Both held detected
   - `test_event_cv_edges` - CV rise/fall detected

3. **Integration tests**
   - `test_mode_cycle` - Mode advances through all states
   - `test_menu_entry_exit` - Menu enters and exits correctly
   - `test_menu_context_aware` - Menu starts at mode-relevant page
   - `test_settings_persist` - Settings saved on menu exit

## File Changes

| File | Change | Description |
|------|--------|-------------|
| `include/fsm/fsm.h` | Create | FSM engine interface |
| `include/fsm/events.h` | Create | Event type definitions |
| `include/fsm/gatekeeper_fsm.h` | Create | Gatekeeper-specific FSM setup |
| `include/fsm/mode_handlers.h` | Create | Mode handler interface and declarations |
| `include/input/cv_input.h` | Create | CV input with ADC and hysteresis |
| `src/fsm/fsm.c` | Create | FSM engine implementation |
| `src/fsm/events.c` | Create | Event processor implementation |
| `src/fsm/gatekeeper_fsm.c` | Create | State/transition definitions, actions |
| `src/fsm/mode_handlers.c` | Create | Mode handler implementations |
| `src/fsm/CMakeLists.txt` | Create | Build configuration |
| `src/input/cv_input.c` | Create | CV input ADC reading with hysteresis |
| `src/CMakeLists.txt` | Modify | Add fsm subdirectory |
| `src/main.c` | Modify | Replace io_controller with gatekeeper_fsm |
| `include/hardware/hal.h` | Modify | Add ADC function declarations |
| `src/hardware/hal.c` | Modify | Add ADC init and read functions |
| `test/unit/fsm/test_fsm.h` | Create | FSM unit tests |
| `test/unit/fsm/test_events.h` | Create | Event processor tests |
| `test/unit/fsm/test_mode_handlers.h` | Create | Mode handler tests |
| `test/unit/input/test_cv_input.h` | Create | CV input conditioning tests |
| `test/unit/CMakeLists.txt` | Modify | Add FSM and CV input test sources |
| `docs/ARCHITECTURE.md` | Modify | Document FSM architecture |

## Dependencies

- AVR PROGMEM support (`<avr/pgmspace.h>`)
- Existing HAL (timer, GPIO, EEPROM)
- Existing app_init module (settings struct, save function)

## Alternatives Considered

### 1. Flat switch-case (current approach)

**Rejected**: Doesn't scale. Adding modes/pages requires modifying multiple switch statements. No clear separation of state logic.

### 2. Function pointer state pattern

Each state is a function that returns the next state:

```c
typedef State (*StateFunc)(Event event);
```

**Rejected**: Less explicit than table-driven. Harder to visualize all transitions. No entry/exit action support without additional complexity.

### 3. Full statechart library

Using a complete UML statechart implementation with history states, orthogonal regions, etc.

**Rejected**: Overkill for this application. Significant code size overhead. The hierarchical FSM approach provides sufficient expressiveness.

### 4. Event queue

Buffer multiple events and process in batch.

**Rejected**: Adds complexity. Single-event-per-tick is sufficient for button/CV input rates. Can revisit if needed for CV clock at high frequencies.

## Design Decisions

The following decisions have been finalized:

### 1. Menu Timeout (Decided)

**Decision**: Menu auto-exits after configurable timeout (default 20 seconds). Settings are saved on exit.

- Timeout duration: 15-30 seconds, user-configurable
- Add `PAGE_MENU_TIMEOUT` to menu pages for configuration
- On timeout: trigger `EVT_MENU_EXIT`, save settings (same as manual exit)
- Timeout resets on any button activity

```c
#define MENU_TIMEOUT_DEFAULT_MS  20000  // 20 seconds
#define MENU_TIMEOUT_MIN_MS      15000  // 15 seconds
#define MENU_TIMEOUT_MAX_MS      30000  // 30 seconds
```

### 2. Performance During Menu (Decided)

**Decision**: CV and button processing **never stops**. The music never stops.

- Mode handlers continue running while in menu
- CV input continues generating events
- Button B performance actions (gate out, pulse trigger, etc.) remain active
- Only menu-specific button gestures are intercepted by menu FSM
- This ensures the module remains musically useful even while adjusting settings

### 3. Mode Handler Architecture (Decided)

**Decision**: Hybrid approach - FSM orchestrates, dedicated handlers process signals.

The FSM owns state and configuration but delegates signal processing to pure mode handlers:

```c
/**
 * LED feedback state returned by mode handlers
 *
 * Each mode can specify what LED X and Y should display
 * based on the mode's internal state.
 */
typedef struct {
    Color led_x_color;      // Mode indicator color (or override)
    LEDState led_y_state;   // LED Y state: off/on/blink/glow
    Color led_y_color;      // LED Y color
} LEDFeedback;

/**
 * Mode handler interface
 *
 * Handlers are pure functions that process input→output.
 * FSM calls appropriate handler based on current mode.
 */
typedef struct {
    /**
     * Process one update cycle.
     * @param ctx    Mode-specific context/config
     * @param input  Current input state (button or CV)
     * @param output Pointer to output state (set by handler)
     * @return       true if output changed
     */
    bool (*process)(void *ctx, bool input, bool *output);

    /**
     * Called when mode becomes active.
     * Initialize mode-specific state.
     */
    void (*on_activate)(void *ctx);

    /**
     * Called when leaving mode.
     * Clean up mode-specific state.
     */
    void (*on_deactivate)(void *ctx);

    /**
     * Get LED feedback for current mode state.
     * Called every update cycle to determine LED display.
     * @param ctx      Mode-specific context
     * @param feedback Pointer to LEDFeedback struct to fill
     */
    void (*get_led_feedback)(void *ctx, LEDFeedback *feedback);
} ModeHandler;

// Handler instances (defined in mode_handlers.c)
extern const ModeHandler gate_handler;
extern const ModeHandler trigger_handler;
extern const ModeHandler toggle_handler;
extern const ModeHandler divide_handler;
extern const ModeHandler cycle_handler;

// Handler lookup table (PROGMEM)
static const ModeHandler* const mode_handlers[] PROGMEM = {
    &gate_handler,      // MODE_GATE
    &trigger_handler,   // MODE_TRIGGER
    &toggle_handler,    // MODE_TOGGLE
    &divide_handler,    // MODE_DIVIDE
    &cycle_handler,     // MODE_CYCLE
};
```

**Benefits:**
- FSM stays lean (orchestration only)
- Handlers are testable in isolation
- Easy to add new modes (implement handler, add to table)
- Clear separation of concerns

**Mode-Specific LED Feedback:**

Each mode handler provides LED feedback appropriate to its function:

| Mode | LED X | LED Y |
|------|-------|-------|
| Gate | Green (mode color) | Mirrors CV output state |
| Trigger | Orange | Flash on pulse trigger |
| Toggle | Cyan | Solid when latched high |
| Divide | Magenta | Blink on divided output (every Nth beat) |
| Cycle | Yellow | Pulse with LFO/tempo phase |

Example implementation for Divide mode:

```c
void divide_get_led_feedback(void *ctx, LEDFeedback *fb) {
    DivideContext *div = (DivideContext *)ctx;

    fb->led_x_color = COLOR_MODE_DIVIDE;

    // Blink LED Y only on the divided output beat
    if (div->output_triggered_this_cycle) {
        fb->led_y_state = LED_STATE_ON;
        fb->led_y_color = COLOR_VALUE_DEFAULT;
    } else {
        fb->led_y_state = LED_STATE_OFF;
        fb->led_y_color = COLOR_OFF;
    }
}
```

The main loop queries the active handler for LED state each cycle:

```c
void gatekeeper_update(void) {
    // ... process events, run mode handler ...

    // Get LED feedback from active mode
    LEDFeedback fb;
    current_handler->get_led_feedback(mode_ctx, &fb);

    // Apply to LEDs (handled by led_feedback module)
    led_feedback_apply(&fb);
}
```

### 4. CV Input Conditioning (Decided)

**Decision**: Use ADC with software hysteresis instead of digital GPIO.

The ATtiny85's ADC on PB3 provides better noise immunity and configurability:

```c
/**
 * CV input processor with hysteresis
 *
 * Reads CV as analog value (10-bit ADC) and applies
 * configurable threshold with hysteresis band to prevent
 * chatter on slow or noisy edges.
 */
typedef struct {
    uint16_t threshold;     // Trigger threshold (0-1023, default 512 = ~2.5V)
    uint16_t hysteresis;    // Hysteresis band (default 50 = ~0.25V)
    bool state;             // Current logical state (high/low)
    bool last_state;        // Previous state for edge detection
} CVInput;

#define CV_THRESHOLD_DEFAULT    512   // ~2.5V with 5V reference
#define CV_HYSTERESIS_DEFAULT   50    // ~0.25V band

/**
 * Initialize CV input processor.
 */
void cv_input_init(CVInput *cv);

/**
 * Read and condition CV input.
 *
 * Applies hysteresis: signal must cross (threshold ± hysteresis)
 * to change state, preventing oscillation on noisy edges.
 *
 * @param cv  CV input processor state
 * @return    true if rising edge detected, false otherwise
 */
bool cv_input_update(CVInput *cv);

/**
 * Implementation
 */
bool cv_input_update(CVInput *cv) {
    uint16_t raw = adc_read(CV_IN_CHANNEL);

    cv->last_state = cv->state;

    if (cv->state) {
        // Currently high - must drop below (threshold - hysteresis) to go low
        if (raw < cv->threshold - cv->hysteresis) {
            cv->state = false;
        }
    } else {
        // Currently low - must rise above (threshold + hysteresis) to go high
        if (raw > cv->threshold + cv->hysteresis) {
            cv->state = true;
        }
    }

    return cv->state;
}
```

**ADC Configuration (8 MHz clock):**
```c
void adc_init(void) {
    // Use VCC as reference, select ADC3 (PB3)
    ADMUX = (0 << REFS0) | (0x03);

    // Enable ADC, prescaler = 64 (125 kHz ADC clock @ 8 MHz)
    // Conversion time: 13 cycles = 104 µs
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1);
}

uint16_t adc_read(uint8_t channel) {
    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
    ADCSRA |= (1 << ADSC);              // Start conversion
    while (ADCSRA & (1 << ADSC));       // Wait for completion
    return ADC;                          // 10-bit result
}
```

**Benefits:**
- Software hysteresis prevents chatter on noisy/slow CV edges
- Configurable threshold (could add menu page for this)
- Future: could use CV level for analog control, not just gate/trigger
- 104 µs conversion time is negligible in main loop

## Open Questions

1. **LED driver integration**: How will the FSM coordinate with the Neopixel driver for page colors and selection states? (See FDP-005)

2. **ADC threshold configurability**: Should CV threshold be user-configurable via menu, or fixed at 2.5V?

3. **Cycle mode tempo range**: What BPM range should the internal clock support?

## Implementation Checklist

- [ ] Create `include/fsm/` directory structure
- [ ] Implement FSM engine (`fsm.c`)
- [ ] Implement event processor (`events.c`)
- [ ] Implement mode handler interface and handlers (`mode_handlers.c`)
- [ ] Implement CV input with ADC and hysteresis (`cv_input.c`)
- [ ] Add ADC functions to HAL
- [ ] Define Gatekeeper states and transitions
- [ ] Implement menu timeout logic
- [ ] Write FSM unit tests
- [ ] Write event processor tests
- [ ] Write mode handler tests
- [ ] Write CV input tests
- [ ] Integrate with main.c
- [ ] Update ARCHITECTURE.md
- [ ] Test on hardware
- [ ] Profile memory usage


## Michael's response

Q's:

1. The neopixel FDP is approved

2. I think we'll want some settings related to this

3. Can we get to 240 BPM safely?