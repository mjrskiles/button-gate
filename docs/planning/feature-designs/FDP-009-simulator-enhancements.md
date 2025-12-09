# FDP-009: Simulator Headless Architecture & Display Enhancements

## Status

Draft

## Summary

Refactor the x86 simulator to separate the simulation backend from display rendering. The backend outputs structured JSON state, which can be consumed by multiple frontends: terminal UI, web UI, test harnesses, or logging tools. This also adds state/mode readout, color legend, and state change logging.

## Motivation

The current simulator tightly couples simulation logic with ANSI terminal rendering in `sim_hal.c`. This makes it difficult to:
- Add alternative display frontends (web UI, GUI)
- Parse simulator output programmatically for testing
- Log structured data for analysis
- Reuse the simulation engine in different contexts

By outputting JSON, we get:
- Clean separation of concerns
- Machine-readable output for tooling
- Easy extension to new frontends
- Consistent schema for all consumers

## Detailed Design

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Simulator Backend                         │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ Coordinator │  │  sim_hal.c  │  │   sim_state.c       │  │
│  │  (app FSM)  │──│ (HAL impl)  │──│ (state aggregator)  │  │
│  └─────────────┘  └─────────────┘  └──────────┬──────────┘  │
└───────────────────────────────────────────────┼─────────────┘
                                                │
                                          JSON State
                                                │
                    ┌───────────────────────────┼───────────────┐
                    │                           │               │
              ┌─────▼─────┐              ┌──────▼──────┐  ┌─────▼─────┐
              │  Terminal │              │    JSON     │  │  Future:  │
              │  Renderer │              │   stdout    │  │  Web UI   │
              │ (ANSI UI) │              │  (--json)   │  │           │
              └───────────┘              └─────────────┘  └───────────┘
```

### JSON Output Schema

Create a versioned schema for simulator state output:

```json
{
  "$schema": "sim_state_v1",
  "version": 1,
  "timestamp_ms": 12345,
  "state": {
    "top": "PERFORM",
    "mode": "GATE",
    "page": null
  },
  "inputs": {
    "button_a": false,
    "button_b": true,
    "cv_in": false
  },
  "outputs": {
    "signal": true
  },
  "leds": [
    {"index": 0, "name": "mode", "r": 0, "g": 255, "b": 0},
    {"index": 1, "name": "activity", "r": 255, "g": 255, "b": 255}
  ],
  "events": [
    {"time_ms": 100, "type": "state_change", "message": "State -> MENU"},
    {"time_ms": 150, "type": "input", "message": "Button A pressed"}
  ]
}
```

### Schema File

Store schema definition in `sim/schema/sim_state_v1.json`:

```json
{
  "$id": "sim_state_v1",
  "type": "object",
  "required": ["version", "timestamp_ms", "state", "inputs", "outputs", "leds"],
  "properties": {
    "version": {"type": "integer", "const": 1},
    "timestamp_ms": {"type": "integer"},
    "state": {
      "type": "object",
      "properties": {
        "top": {"enum": ["PERFORM", "MENU"]},
        "mode": {"enum": ["GATE", "TRIGGER", "TOGGLE", "DIVIDE", "CYCLE"]},
        "page": {"type": ["string", "null"]}
      }
    },
    "inputs": {
      "type": "object",
      "properties": {
        "button_a": {"type": "boolean"},
        "button_b": {"type": "boolean"},
        "cv_in": {"type": "boolean"}
      }
    },
    "outputs": {
      "type": "object",
      "properties": {
        "signal": {"type": "boolean"}
      }
    },
    "leds": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "index": {"type": "integer"},
          "name": {"type": "string"},
          "r": {"type": "integer", "minimum": 0, "maximum": 255},
          "g": {"type": "integer", "minimum": 0, "maximum": 255},
          "b": {"type": "integer", "minimum": 0, "maximum": 255}
        }
      }
    },
    "events": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "time_ms": {"type": "integer"},
          "type": {"enum": ["state_change", "mode_change", "page_change", "input", "output", "info"]},
          "message": {"type": "string"}
        }
      }
    }
  }
}
```

### Output Modes

Command-line flags select the output renderer:

| Flag | Output Mode | Description |
|------|-------------|-------------|
| (default) | Terminal | ANSI UI with colors, refreshes in place |
| `--json` | JSON | One JSON object per state change, newline-delimited |
| `--json-stream` | JSON Stream | Continuous JSON objects at fixed interval |
| `--batch` | Plain text | Existing batch mode for scripts (events only) |

### New File Structure

```
sim/
├── sim_main.c           # Entry point, argument parsing
├── sim_hal.c            # HAL implementation (hardware simulation)
├── sim_hal.h
├── sim_state.c          # State aggregator - collects all state into struct
├── sim_state.h          # SimState struct definition
├── sim_neopixel.c       # Neopixel driver for sim
├── input_source.c       # Keyboard/script input
├── input_source.h
├── render/
│   ├── render.h         # Renderer interface
│   ├── render_terminal.c # ANSI terminal renderer
│   └── render_json.c    # JSON renderer
└── schema/
    └── sim_state_v1.json # JSON schema definition
```

### SimState Structure

Central state structure passed to renderers:

```c
// sim/sim_state.h
#ifndef SIM_STATE_H
#define SIM_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/states.h"

#define SIM_MAX_EVENTS 16
#define SIM_NUM_LEDS 2

typedef enum {
    EVT_STATE_CHANGE,
    EVT_MODE_CHANGE,
    EVT_PAGE_CHANGE,
    EVT_INPUT,
    EVT_OUTPUT,
    EVT_INFO
} SimEventType;

typedef struct {
    uint32_t time_ms;
    SimEventType type;
    char message[128];
} SimEvent;

typedef struct {
    uint8_t r, g, b;
} SimLED;

typedef struct {
    // Schema version
    int version;

    // Timestamp
    uint32_t timestamp_ms;

    // FSM state
    TopState top_state;
    ModeState mode;
    MenuPage page;
    bool in_menu;

    // Inputs
    bool button_a;
    bool button_b;
    bool cv_in;

    // Outputs
    bool signal_out;

    // LEDs
    SimLED leds[SIM_NUM_LEDS];

    // Recent events (circular buffer)
    SimEvent events[SIM_MAX_EVENTS];
    int event_head;
    int event_count;

    // Display hints (for terminal renderer)
    bool realtime_mode;
    bool legend_visible;
} SimState;

// Initialize state struct
void sim_state_init(SimState *state);

// Update state from various sources
void sim_state_set_fsm(SimState *state, TopState top, ModeState mode, MenuPage page, bool in_menu);
void sim_state_set_inputs(SimState *state, bool btn_a, bool btn_b, bool cv);
void sim_state_set_output(SimState *state, bool signal);
void sim_state_set_led(SimState *state, int index, uint8_t r, uint8_t g, uint8_t b);
void sim_state_add_event(SimState *state, SimEventType type, const char *fmt, ...);

// Check if state changed since last render
bool sim_state_is_dirty(const SimState *state);
void sim_state_clear_dirty(SimState *state);

#endif
```

### Renderer Interface

```c
// sim/render/render.h
#ifndef SIM_RENDER_H
#define SIM_RENDER_H

#include "sim_state.h"

typedef struct Renderer Renderer;

struct Renderer {
    void (*init)(Renderer *self);
    void (*render)(Renderer *self, const SimState *state);
    void (*cleanup)(Renderer *self);
    void *ctx;  // Renderer-specific context
};

// Create renderers
Renderer* render_terminal_create(void);
Renderer* render_json_create(bool stream_mode);

#endif
```

### Terminal Renderer Features

The terminal renderer includes the originally planned enhancements:

1. **State/Mode Readout Line**
```
  State: PERFORM     Mode: GATE     Page: --
```

2. **Toggleable Legend** (press `[L]`)
```
┌─ LED Color Legend ──────────────────────────────┐
│  Mode LED:                                       │
│    Green     GATE      Cyan      TRIGGER        │
│    Orange    TOGGLE    Magenta   DIVIDE         │
│    Blue      CYCLE                              │
│  Activity LED:                                  │
│    White     Output active                      │
└─────────────────────────────────────────────────┘
```

3. **State Change Events** in log

### JSON Renderer Output

Output follows the [NDJSON](https://github.com/ndjson/ndjson-spec) convention (Newline Delimited JSON) - one complete JSON object per line. This is a format convention, not a library dependency. We hand-write the JSON output using `printf()`.

```
{"version":1,"timestamp_ms":0,"state":{"top":"PERFORM","mode":"GATE","page":null},...}
{"version":1,"timestamp_ms":100,"state":{"top":"MENU","mode":"GATE","page":"GATE_CV"},...}
```

### Main Loop Integration

```c
// sim_main.c
int main(int argc, char **argv) {
    // Parse args, select renderer
    Renderer *renderer = parse_args(argc, argv);

    // Initialize
    SimState state;
    sim_state_init(&state);
    renderer->init(renderer);

    // ... coordinator init ...

    while (running) {
        // Update inputs
        input_source->update(...);

        // Run coordinator
        coordinator_update(&coordinator);

        // Collect state
        sim_state_set_fsm(&state,
            coordinator_get_top_state(&coordinator),
            coordinator_get_mode(&coordinator),
            coordinator_get_page(&coordinator),
            coordinator_in_menu(&coordinator));

        sim_state_set_output(&state, coordinator_get_output(&coordinator));

        // Update LEDs...

        // Render if dirty
        if (sim_state_is_dirty(&state)) {
            renderer->render(renderer, &state);
            sim_state_clear_dirty(&state);
        }

        sim_tick();
    }

    renderer->cleanup(renderer);
}
```

### Event Types

| Type | Description | Example |
|------|-------------|---------|
| `state_change` | Top-level FSM transition | "State -> MENU" |
| `mode_change` | Mode selection changed | "Mode -> TRIGGER" |
| `page_change` | Menu page navigation | "Page -> DIVIDE_DIVISOR" |
| `input` | Button/CV input event | "Button A pressed" |
| `output` | Signal output change | "Output -> HIGH" |
| `info` | General info | "Simulator started" |

## File Changes

| File | Change | Description |
|------|--------|-------------|
| `include/core/coordinator.h` | Modify | Add `coordinator_get_top_state()` |
| `src/core/coordinator.c` | Modify | Implement `coordinator_get_top_state()` |
| `sim/sim_state.h` | Create | SimState struct and API |
| `sim/sim_state.c` | Create | State aggregation implementation |
| `sim/render/render.h` | Create | Renderer interface |
| `sim/render/render_terminal.c` | Create | ANSI terminal renderer (refactored from sim_hal.c) |
| `sim/render/render_json.c` | Create | JSON output renderer |
| `sim/schema/sim_state_v1.json` | Create | JSON schema definition |
| `sim/sim_hal.c` | Modify | Remove display code, keep HAL functions |
| `sim/sim_hal.h` | Modify | Remove display functions |
| `sim/sim_main.c` | Modify | Use renderer interface, state aggregation |
| `sim/input_source.c` | Modify | Add `[L]` legend toggle |
| `sim/CMakeLists.txt` | Modify | Add new source files |

## Dependencies

**None.** This feature adds no new dependencies:

- **No JSON library** - JSON output is hand-written with `printf()`. NDJSON is a format convention, not a library.
- **No schema validation** - The JSON schema file (`sim/schema/sim_state_v1.json`) is for documentation and external tools (e.g., validating with `ajv` or `jsonschema`), not used at runtime.

## Alternatives Considered

### 1. Use cJSON Library

Add cJSON for proper JSON generation.

**Rejected**: Adds dependency for simple output. Hand-crafted JSON is sufficient and keeps binary small.

### 2. Protocol Buffers

Use protobuf for structured output.

**Rejected**: Overkill for this use case. JSON is human-readable and widely supported.

### 3. Keep Coupled Architecture

Just add features to existing `sim_hal.c`.

**Rejected**: Growing technical debt. Separation now enables future frontends.

## Open Questions

1. **JSON output frequency**: Every state change, or periodic snapshots? Starting with state-change-triggered output.

2. **Schema evolution**: How to handle v2 schema? Include version field, document breaking changes.

3. **Event buffer size**: 16 events enough? Could make configurable.

## Implementation Checklist

- [ ] Add `coordinator_get_top_state()` to coordinator
- [ ] Create `sim/sim_state.h` with SimState struct
- [ ] Create `sim/sim_state.c` with state management
- [ ] Create `sim/render/render.h` interface
- [ ] Create `sim/render/render_terminal.c` (extract from sim_hal.c)
- [ ] Create `sim/render/render_json.c`
- [ ] Create `sim/schema/sim_state_v1.json`
- [ ] Refactor `sim_hal.c` to remove display code
- [ ] Update `sim_main.c` for new architecture
- [ ] Add `--json` and `--json-stream` flags
- [ ] Add `[L]` legend toggle to terminal renderer
- [ ] Add state line to terminal renderer
- [ ] Add state change logging
- [ ] Update CMakeLists.txt
- [ ] Test terminal mode
- [ ] Test JSON mode
- [ ] Test batch mode still works
- [ ] Update `--help` text
