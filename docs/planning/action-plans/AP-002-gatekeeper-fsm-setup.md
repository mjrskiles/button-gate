# AP-002: Gatekeeper FSM Setup

**Status: Complete**

> **Note:** This action plan was written during initial implementation. The code has since been
> restructured: `gatekeeper_fsm.*` → `coordinator.*`, files moved from `fsm/` to `core/`, `events/`, `modes/`.
> See CLAUDE.md for current file locations.

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

## Implementation (As Built)

### Current File Structure

| Planned | Implemented As |
|---------|----------------|
| `include/fsm/gatekeeper_states.h` | `include/core/states.h` |
| `include/fsm/gatekeeper_fsm.h` | `include/core/coordinator.h` |
| `src/fsm/gatekeeper_fsm.c` | `src/core/coordinator.c` |
| `include/fsm/events.h` | `include/events/events.h` |
| `src/fsm/events.c` | `src/events/events.c` |

### Key Types

```c
// State definitions (include/core/states.h)
typedef enum { TOP_PERFORM, TOP_MENU, TOP_STATE_COUNT } TopState;
typedef enum { MODE_GATE, MODE_TRIGGER, MODE_TOGGLE, MODE_DIVIDE, MODE_CYCLE, MODE_COUNT } ModeState;
typedef enum { PAGE_GATE_CV, PAGE_TRIGGER_BEHAVIOR, /* ... */, PAGE_COUNT } MenuPage;

// Coordinator (include/core/coordinator.h)
typedef struct {
    FSM top_fsm;            // PERFORM ↔ MENU
    FSM mode_fsm;           // Mode selection
    FSM menu_fsm;           // Menu page navigation
    EventProcessor events;  // Input event detection
    ModeContext mode_ctx;   // Mode handler state
    // ... context fields
} Coordinator;

void coordinator_init(Coordinator *coord, AppSettings *settings);
void coordinator_start(Coordinator *coord);
void coordinator_update(Coordinator *coord);
ModeState coordinator_get_mode(const Coordinator *coord);
void coordinator_set_mode(Coordinator *coord, ModeState mode);
bool coordinator_get_output(const Coordinator *coord);
```

### Main Loop Integration

```c
// src/main.c
#include "core/coordinator.h"

static Coordinator coordinator;
static AppSettings settings;

int main(void) {
    p_hal->init();
    app_init_run(&settings);

    coordinator_init(&coordinator, &settings);
    coordinator_set_mode(&coordinator, (ModeState)settings.mode);
    coordinator_start(&coordinator);

    while (1) {
        coordinator_update(&coordinator);

        if (coordinator_get_output(&coordinator)) {
            p_hal->set_pin(p_hal->sig_out_pin);
        } else {
            p_hal->clear_pin(p_hal->sig_out_pin);
        }
    }
}
```

## Success Criteria

- [x] Top-level FSM switches between Perform and Menu
- [x] Mode FSM cycles through all 5 modes
- [x] Menu FSM navigates through all pages
- [x] Menu timeout triggers exit
- [x] Context-aware menu entry works
- [x] All existing tests still pass (133 tests total)
- [x] Simulator works with coordinator-based main loop

## Dependencies

- AP-001: FSM Engine (complete)
- `app_init` module for settings

## Notes

- Mode handlers implemented in AP-003
- LED feedback pending AP-004
- File structure refactored for clarity (see CLAUDE.md)
