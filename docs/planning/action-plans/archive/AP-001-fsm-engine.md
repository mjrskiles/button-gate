# AP-001: FSM Engine Implementation

**Status: Complete**

## Overview

Implement the hierarchical finite state machine engine as specified in ADR-003 and FDP-004. This is the foundation for Gatekeeper's state management architecture.

## Scope

This action plan covers:
- FSM engine core (reusable, table-driven)
- Event processor (button gestures, CV edge detection)
- Unit tests for both components

**Not in scope** (future action plans):
- Gatekeeper-specific FSM setup (states, transitions)
- Mode handlers
- Integration with main.c

## Implementation Steps

### Phase 1: Directory Structure & Headers

Create the FSM module structure:

```
include/fsm/
├── fsm.h           # FSM engine interface
├── events.h        # Event types and processor interface
src/fsm/
├── fsm.c           # FSM engine implementation
├── events.c        # Event processor implementation
test/unit/fsm/
├── test_fsm.h      # FSM engine tests
├── test_events.h   # Event processor tests
```

### Phase 2: FSM Engine Core

**File: `include/fsm/fsm.h`**

Core types:
- `State` - id, entry/exit/update function pointers
- `Transition` - from_state, event, to_state, action pointer
- `FSM` - states, transitions, current state, active flag

Core functions:
- `fsm_init()` - Initialize FSM with state/transition tables
- `fsm_process_event()` - Process event, execute transitions
- `fsm_update()` - Run current state's update function
- `fsm_reset()` - Return to initial state
- `fsm_get_state()` / `fsm_set_state()` - State accessors

Special values:
- `FSM_NO_TRANSITION` (0xFF) - Event handled, no state change
- `FSM_ANY_STATE` (0xFE) - Transition matches any current state

**File: `src/fsm/fsm.c`**

Implementation notes:
- Tables accessed via standard pointers (PROGMEM deferred to hardware integration)
- Linear search through transition table (sufficient for small tables)
- Entry/exit actions called in correct order during transitions
- NULL function pointers safely handled

### Phase 3: Event Processor

**File: `include/fsm/events.h`**

Event types:
```c
typedef enum {
    EVT_NONE = 0,
    // Performance events (on press)
    EVT_A_PRESS, EVT_B_PRESS,
    EVT_CV_RISE, EVT_CV_FALL,
    // Config events (on release)
    EVT_A_TAP, EVT_A_RELEASE,
    EVT_B_TAP, EVT_B_RELEASE,
    // Hold events
    EVT_A_HOLD, EVT_B_HOLD,
    // Compound gestures
    EVT_MENU_ENTER,    // A:hold + B:tap
    EVT_MENU_EXIT,     // Release A while in menu
    EVT_MODE_CHANGE,   // B:hold + A:hold
    // Timing
    EVT_TIMEOUT,
    EVT_COUNT
} Event;
```

EventProcessor struct (using status word per ADR-002):
```c
#define EP_A_PRESSED    (1 << 0)
#define EP_A_LAST       (1 << 1)
#define EP_A_HOLD       (1 << 2)
#define EP_B_PRESSED    (1 << 3)
#define EP_B_LAST       (1 << 4)
#define EP_B_HOLD       (1 << 5)
#define EP_CV_STATE     (1 << 6)
#define EP_CV_LAST      (1 << 7)

typedef struct {
    uint8_t status;
    uint32_t a_press_time;
    uint32_t b_press_time;
} EventProcessor;
```

**File: `src/fsm/events.c`**

Implementation:
- `event_processor_init()` - Initialize processor state
- `event_processor_update()` - Read inputs, detect edges, return event
- Gesture detection logic for compound events
- Configurable hold threshold (default 500ms)
- Configurable tap threshold (default 300ms)

### Phase 4: Unit Tests

**FSM Engine Tests** (`test/unit/fsm/test_fsm.h`):
- `test_fsm_init` - Verify initial state
- `test_fsm_transition_basic` - Simple state transition
- `test_fsm_transition_with_action` - Action called during transition
- `test_fsm_entry_exit_actions` - Entry/exit called in order
- `test_fsm_no_matching_transition` - Unhandled event returns false
- `test_fsm_any_state_wildcard` - FSM_ANY_STATE matches any state
- `test_fsm_no_transition_action` - Action without state change
- `test_fsm_update_calls_state_update` - Update function called
- `test_fsm_reset` - Reset returns to initial state
- `test_fsm_set_state_direct` - Direct state jump

**Event Processor Tests** (`test/unit/fsm/test_events.h`):
- `test_event_init` - Verify initial state
- `test_event_a_press` - Detect button A press
- `test_event_a_tap` - Detect short press + release
- `test_event_a_hold` - Detect hold after threshold
- `test_event_b_press` - Detect button B press
- `test_event_cv_rise` - Detect CV rising edge
- `test_event_cv_fall` - Detect CV falling edge
- `test_event_menu_enter` - A:hold + B:tap compound
- `test_event_mode_change` - Both buttons held
- `test_event_no_double_fire` - Events fire once per edge

## File Changes Summary

| File | Action | Description |
|------|--------|-------------|
| `include/fsm/fsm.h` | Create | FSM engine interface |
| `include/fsm/events.h` | Create | Event types and processor |
| `src/fsm/fsm.c` | Create | FSM engine implementation |
| `src/fsm/events.c` | Create | Event processor implementation |
| `src/CMakeLists.txt` | Modify | Add fsm sources (for future) |
| `test/unit/fsm/test_fsm.h` | Create | FSM engine tests |
| `test/unit/fsm/test_events.h` | Create | Event processor tests |
| `test/unit/unit_tests.c` | Modify | Include new test groups |
| `test/unit/CMakeLists.txt` | Modify | Add fsm test sources |

## Testing Strategy

1. **Unit tests first** - Write tests before implementation where practical
2. **Mock HAL** - Use existing mock_hal for timing and pin reads
3. **Edge cases** - Test boundary conditions (exact thresholds, etc.)
4. **Isolation** - FSM engine tests don't depend on event processor

## Success Criteria

- [x] All new tests pass (33 new tests)
- [x] Existing 74 tests still pass (107 total)
- [x] FSM engine handles all specified operations
- [x] Event processor detects all event types
- [x] No memory leaks or buffer overflows
- [x] Code compiles without warnings

## Dependencies

- `utility/status.h` - STATUS_* macros (ADR-002, implemented)
- `hardware/hal_interface.h` - HAL for timing and GPIO
- Mock HAL for testing

## Estimated Scope

- ~200 lines header code
- ~300 lines implementation code
- ~400 lines test code
