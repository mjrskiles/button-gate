# AP-002: Gatekeeper FSM Setup

**Status: Complete**

## Overview

Wire up the FSM engine (AP-001) with Gatekeeper-specific states, transitions, and integration with main.c. This creates the hierarchical FSM structure: Top-level (Perform/Menu) → Mode FSM → Menu FSM.

## Prerequisites

- [x] AP-001: FSM Engine (complete)
- [x] ADR-003: FSM Architecture (accepted)
- [x] FDP-004: State Machine Module (proposed)

## Scope

**In scope:**
- Top-level FSM (PERFORM ↔ MENU states)
- Mode FSM (Gate, Trigger, Toggle, Divide, Cycle)
- Menu FSM (settings pages as flat ring)
- State/transition table definitions
- Integration with main.c
- Event processor wired to HAL inputs

**Not in scope:**
- Mode handler signal processing (AP-003)
- Neopixel LED feedback (AP-004)
- Full menu functionality (settings persistence)

## Implementation Steps

### Phase 1: State & Event Definitions

**File: `include/fsm/gatekeeper_states.h`**

```c
// Top-level states
typedef enum {
    TOP_PERFORM = 0,
    TOP_MENU,
    TOP_STATE_COUNT
} TopState;

// Mode states
typedef enum {
    MODE_GATE = 0,
    MODE_TRIGGER,
    MODE_TOGGLE,
    MODE_DIVIDE,
    MODE_CYCLE,
    MODE_COUNT
} ModeState;

// Menu pages (flat ring)
typedef enum {
    PAGE_GATE_MODE = 0,
    PAGE_TRIGGER_BEHAVIOR,
    PAGE_TRIGGER_PULSE_LEN,
    PAGE_TOGGLE_MODE,
    PAGE_DIVIDER_DIVISOR,
    PAGE_CYCLE_BEHAVIOR,
    PAGE_CV_GLOBAL,
    PAGE_MENU_TIMEOUT,
    PAGE_COUNT
} MenuPage;
```

### Phase 2: Gatekeeper FSM Coordinator

**File: `include/fsm/gatekeeper_fsm.h`**

```c
typedef struct {
    FSM top_fsm;            // PERFORM ↔ MENU
    FSM mode_fsm;           // Mode selection
    FSM menu_fsm;           // Menu page navigation
    EventProcessor events;  // Input event detection

    // Context
    uint8_t menu_entry_mode;    // Mode when menu was entered
    uint32_t menu_enter_time;   // For timeout
} GatekeeperFSM;

void gatekeeper_fsm_init(GatekeeperFSM *gk);
void gatekeeper_fsm_start(GatekeeperFSM *gk);
void gatekeeper_fsm_update(GatekeeperFSM *gk);
```

**File: `src/fsm/gatekeeper_fsm.c`**

- State tables for top/mode/menu FSMs
- Transition tables with actions
- Entry/exit actions (menu context jump, timeout tracking)
- Main update loop routing events to appropriate FSM

### Phase 3: Transition Tables

**Top-level transitions:**
```c
const Transition top_transitions[] = {
    { TOP_PERFORM, EVT_MENU_ENTER, TOP_MENU, action_enter_menu },
    { TOP_MENU, EVT_MENU_EXIT, TOP_PERFORM, action_exit_menu },
    { TOP_MENU, EVT_TIMEOUT, TOP_PERFORM, action_exit_menu },
};
```

**Mode transitions:**
```c
const Transition mode_transitions[] = {
    { FSM_ANY_STATE, EVT_MODE_CHANGE, FSM_NO_TRANSITION, action_next_mode },
};
```

**Menu transitions:**
```c
const Transition menu_transitions[] = {
    { FSM_ANY_STATE, EVT_A_TAP, FSM_NO_TRANSITION, action_next_page },
    { FSM_ANY_STATE, EVT_B_TAP, FSM_NO_TRANSITION, action_cycle_value },
};
```

### Phase 4: Action Functions

```c
// Menu entry - jump to mode-relevant page
static void action_enter_menu(void) {
    gk->menu_entry_mode = fsm_get_state(&gk->mode_fsm);
    gk->menu_enter_time = p_hal->millis();

    // Context-aware page jump
    uint8_t page = mode_to_start_page[gk->menu_entry_mode];
    fsm_set_state(&gk->menu_fsm, page);
}

// Menu exit - save settings
static void action_exit_menu(void) {
    // Settings save handled by app_init module
    app_init_save_settings(&settings);
}

// Mode advance (ring)
static void action_next_mode(void) {
    uint8_t next = (fsm_get_state(&gk->mode_fsm) + 1) % MODE_COUNT;
    fsm_set_state(&gk->mode_fsm, next);
}

// Page advance (ring)
static void action_next_page(void) {
    uint8_t next = (fsm_get_state(&gk->menu_fsm) + 1) % PAGE_COUNT;
    fsm_set_state(&gk->menu_fsm, next);
}
```

### Phase 5: Main Loop Integration

**File: `src/main.c` (modifications)**

```c
#include "fsm/gatekeeper_fsm.h"

static GatekeeperFSM gk_fsm;
static AppSettings settings;

int main(void) {
    // Initialize HAL
    hal_init();

    // Load settings
    app_init_load(&settings);

    // Initialize FSM
    gatekeeper_fsm_init(&gk_fsm);
    gatekeeper_fsm_start(&gk_fsm);

    // Set initial mode from settings
    fsm_set_state(&gk_fsm.mode_fsm, settings.mode);

    while (1) {
        gatekeeper_fsm_update(&gk_fsm);
    }
}
```

### Phase 6: Menu Timeout

```c
// In gatekeeper_fsm_update()
if (fsm_get_state(&gk->top_fsm) == TOP_MENU) {
    uint32_t elapsed = p_hal->millis() - gk->menu_enter_time;
    if (elapsed >= MENU_TIMEOUT_MS) {
        fsm_process_event(&gk->top_fsm, EVT_TIMEOUT);
    }
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `include/fsm/gatekeeper_states.h` | Create | State/page enums |
| `include/fsm/gatekeeper_fsm.h` | Create | Coordinator interface |
| `src/fsm/gatekeeper_fsm.c` | Create | FSM setup and coordination |
| `src/main.c` | Modify | Replace io_controller with gatekeeper_fsm |
| `test/unit/fsm/test_gatekeeper_fsm.h` | Create | Integration tests |
| `test/unit/CMakeLists.txt` | Modify | Add new source |

## Testing Strategy

1. **State transition tests** - Verify all transitions work
2. **Menu timeout test** - Verify auto-exit after timeout
3. **Context-aware menu entry** - Verify correct page on entry
4. **Mode persistence** - Mode survives menu entry/exit
5. **Event routing** - Events reach correct FSM

## Success Criteria

- [x] Top-level FSM switches between Perform and Menu
- [x] Mode FSM cycles through all 5 modes
- [x] Menu FSM navigates through all pages
- [x] Menu timeout triggers exit
- [x] Context-aware menu entry works
- [x] All existing tests still pass (112 tests)
- [x] Simulator works with FSM-based main loop

## Dependencies

- AP-001: FSM Engine (complete)
- `app_init` module for settings

## Notes

- Mode handlers (AP-003) will be stubbed initially
- LED feedback (AP-004) will be added later
- This creates the skeleton that other components plug into
