---
type: fdp
id: FDP-011
status: in progress
created: 2025-12-15
modified: '2025-12-15'
supersedes: null
superseded_by: null
obsoleted_by: null
related:
- FDP-010
---

# FDP-011: Simulator Socket Architecture

## Status

In Progress

## Summary

Add Unix domain socket server to the simulator, enabling external frontends to send commands and receive state updates. Move CV signal generation into the simulator core with configurable sources (LFO, envelope, wavetable, manual). This decouples input handling from the simulator, allowing richer frontends (gamepad support, custom GUIs) while keeping timing deterministic.

## Motivation

Current limitations:
1. **Terminal input constraints** - Unix terminals don't provide key release events, making gesture testing awkward
2. **No gamepad support** - Would need SDL or similar, which is heavyweight for a CLI tool
3. **CV input is primitive** - Just `sim_set_cv_voltage(value)`, no way to simulate LFOs, envelopes, or complex modulation
4. **Tight coupling** - Input handling is embedded in `input_source.c`, mixing keyboard I/O with sim control

Goals:
1. **Deterministic CV generation** - Sim owns timing, produces reproducible results
2. **External frontend support** - Any language can connect via socket (Python/pygame for gamepad, browser for Gamepad API, etc.)
3. **Rich CV sources** - LFO, envelope, wavetable, manual control with parameters
4. **Testability** - Scripts can configure CV sources, assert on behavior

## Detailed Design

### Overview

```
┌────────────────────────────────────────┐
│         Frontend (Python/etc)          │
│  ┌─────────┐ ┌─────────┐ ┌──────────┐  │
│  │ Gamepad │ │   LFO   │ │Wavetable │  │
│  │ Handler │ │ Controls│ │  Editor  │  │
│  └────┬────┘ └────┬────┘ └────┬─────┘  │
│       └───────────┴───────────┘        │
│                   │                    │
│           JSON Commands                │
└───────────────────┼────────────────────┘
                    │ Unix Socket (bidirectional)
                    ▼
┌───────────────────┼────────────────────┐
│              Simulator                 │
│  ┌────────────────▼─────────────────┐  │
│  │      Socket Command Handler      │  │
│  └────────────────┬─────────────────┘  │
│                   │                    │
│  ┌────────────────▼─────────────────┐  │
│  │       CV Source Manager          │  │
│  │  ┌─────┐ ┌────────┐ ┌─────────┐  │  │
│  │  │ LFO │ │Envelope│ │Wavetable│  │  │
│  │  └──┬──┘ └───┬────┘ └────┬────┘  │  │
│  │     └────────┴───────────┘       │  │
│  │              │ cv_value          │  │
│  └──────────────┼───────────────────┘  │
│                 ▼                      │
│    sim_set_cv_voltage() ← existing    │
│                 │                      │
│         Coordinator loop               │
└────────────────────────────────────────┘
```

**Key principle**: The simulator owns all timing. Frontends send *parameters* (e.g., "LFO at 2Hz"), not samples. The sim generates CV values each tick, ensuring determinism.

### Components

#### 1. Socket Server

Unix domain socket listener, non-blocking. Accepts one client at a time (single frontend).

```c
// sim/socket_server.h
typedef struct SocketServer SocketServer;

SocketServer* socket_server_create(const char *path);
void socket_server_destroy(SocketServer *server);

// Non-blocking: returns true if command was received
bool socket_server_poll(SocketServer *server, char *cmd_buf, size_t buf_size);

// Send state update to connected client
void socket_server_send(SocketServer *server, const char *json);

// Check if client is connected
bool socket_server_connected(SocketServer *server);
```

Default socket path: `/tmp/gatekeeper-sim.sock`

#### 2. CV Source Manager

Manages active CV source and generates values each tick.

```c
// sim/cv_source.h

typedef enum {
    CV_SOURCE_MANUAL,      // Fixed value
    CV_SOURCE_LFO,         // Oscillator
    CV_SOURCE_ENVELOPE,    // ADSR
    CV_SOURCE_WAVETABLE,   // Custom shape
    CV_SOURCE_SEQUENCE     // Step sequence (future)
} CVSourceType;

typedef struct {
    float freq_hz;         // 0.01 - 100 Hz
    uint8_t shape;         // SINE, TRI, SAW, SQUARE, RANDOM
    uint8_t min_val;       // 0-255
    uint8_t max_val;       // 0-255
    float phase;           // Internal: 0.0-1.0
} LFOParams;

typedef struct {
    uint16_t attack_ms;
    uint16_t decay_ms;
    uint8_t sustain;       // 0-255 level
    uint16_t release_ms;
    uint8_t state;         // Internal: IDLE, ATTACK, DECAY, SUSTAIN, RELEASE
    uint32_t state_start;  // Internal: timestamp
    bool gate;             // Internal: gate state
} EnvelopeParams;

typedef struct {
    uint8_t *samples;      // Sample buffer (caller-owned or static)
    uint16_t length;       // Number of samples
    float freq_hz;         // Playback rate
    float position;        // Internal: current position
} WavetableParams;

typedef struct CVSource {
    CVSourceType type;
    union {
        uint8_t manual_value;
        LFOParams lfo;
        EnvelopeParams envelope;
        WavetableParams wavetable;
    };
} CVSource;

void cv_source_init(CVSource *src);
void cv_source_set_manual(CVSource *src, uint8_t value);
void cv_source_set_lfo(CVSource *src, float freq_hz, uint8_t shape, uint8_t min, uint8_t max);
void cv_source_set_envelope(CVSource *src, uint16_t a, uint16_t d, uint8_t s, uint16_t r);
void cv_source_set_wavetable(CVSource *src, const uint8_t *samples, uint16_t len, float freq_hz);

// Call each tick - returns current CV value (0-255)
uint8_t cv_source_tick(CVSource *src, uint32_t delta_ms);

// Envelope control
void cv_source_gate_on(CVSource *src);
void cv_source_gate_off(CVSource *src);
void cv_source_trigger(CVSource *src);  // One-shot (gate on then off)
```

#### 3. Command Protocol

NDJSON over socket. Each line is a complete JSON command or response.

**Frontend → Sim (commands):**

```json
{"cmd": "button", "id": "a", "state": true}
{"cmd": "button", "id": "b", "state": false}

{"cmd": "cv_manual", "value": 180}

{"cmd": "cv_lfo", "freq_hz": 2.0, "shape": "sine", "min": 0, "max": 255}
{"cmd": "cv_lfo", "freq_hz": 0.5, "shape": "square", "min": 50, "max": 200}

{"cmd": "cv_envelope", "attack_ms": 10, "decay_ms": 100, "sustain": 180, "release_ms": 500}
{"cmd": "cv_gate", "state": true}
{"cmd": "cv_gate", "state": false}
{"cmd": "cv_trigger"}

{"cmd": "cv_wavetable", "samples": [0, 64, 128, 255, 255, 128, 64, 0], "freq_hz": 1.0}

{"cmd": "reset"}
{"cmd": "time_scale", "factor": 10.0}
{"cmd": "quit"}
```

**Sim → Frontend (state):**

Extends existing JSON output schema (`sim/schema/sim_state_v1.json`):

```json
{
  "version": 1,
  "timestamp_ms": 1234,
  "state": {"top": "PERFORM", "mode": "GATE", "page": null},
  "inputs": {
    "button_a": false,
    "button_b": true,
    "cv_in": true,
    "cv_voltage": 180
  },
  "cv_source": {
    "type": "lfo",
    "params": {"freq_hz": 2.0, "shape": "sine", "min": 0, "max": 255},
    "phase": 0.75
  },
  "outputs": {"signal": true},
  "leds": [
    {"index": 0, "name": "mode", "r": 0, "g": 255, "b": 0},
    {"index": 1, "name": "activity", "r": 255, "g": 255, "b": 255}
  ],
  "events": []
}
```

#### 4. Integration with Sim Main Loop

```c
// sim_main.c changes

int main(int argc, char **argv) {
    // ... existing setup ...

    // New: Create socket server if --socket flag
    SocketServer *socket = NULL;
    if (args.socket_mode) {
        socket = socket_server_create(args.socket_path);
    }

    // New: CV source (replaces simple manual value)
    CVSource cv_source;
    cv_source_init(&cv_source);

    while (running) {
        // Process socket commands (non-blocking)
        if (socket) {
            char cmd[512];
            while (socket_server_poll(socket, cmd, sizeof(cmd))) {
                handle_socket_command(cmd, &cv_source);
            }
        }

        // Update CV from source
        uint8_t cv_val = cv_source_tick(&cv_source, 1);  // 1ms tick
        sim_set_cv_voltage(cv_val);

        // ... existing coordinator update ...

        // Send state to socket client
        if (socket && socket_server_connected(socket)) {
            char *json = render_json_state(&sim_state, &cv_source);
            socket_server_send(socket, json);
        }

        // ... existing rendering ...
    }
}
```

### LFO Shape Functions

```c
// Returns -1.0 to 1.0
static float lfo_shape(uint8_t shape, float phase) {
    switch (shape) {
        case LFO_SINE:
            return sinf(phase * 2.0f * M_PI);
        case LFO_TRI:
            return (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
        case LFO_SAW:
            return 2.0f * phase - 1.0f;
        case LFO_SQUARE:
            return (phase < 0.5f) ? 1.0f : -1.0f;
        case LFO_RANDOM:
            // Sample-and-hold: new random value each cycle
            // Implementation: track last_phase, generate on wrap
        default:
            return 0.0f;
    }
}

uint8_t cv_source_tick(CVSource *src, uint32_t delta_ms) {
    if (src->type == CV_SOURCE_LFO) {
        LFOParams *lfo = &src->lfo;
        lfo->phase += (lfo->freq_hz * delta_ms) / 1000.0f;
        while (lfo->phase >= 1.0f) lfo->phase -= 1.0f;

        float normalized = lfo_shape(lfo->shape, lfo->phase);
        float scaled = (normalized + 1.0f) * 0.5f;  // 0.0 to 1.0
        return lfo->min_val + (uint8_t)(scaled * (lfo->max_val - lfo->min_val));
    }
    // ... other source types ...
}
```

### Envelope State Machine

```
         gate_on
  IDLE ──────────► ATTACK
    ▲                │
    │                ▼ (level reaches max)
    │              DECAY
    │                │
    │                ▼ (level reaches sustain)
    │             SUSTAIN ◄─────┐
    │                │          │
    │    gate_off    │ gate_on  │
    │◄───────────────┤          │
    │                ▼          │
    │             RELEASE ──────┘
    │                │    (retrigger)
    └────────────────┘
      (level reaches 0)
```

## File Changes

| File | Change | Description |
|------|--------|-------------|
| `sim/socket_server.h` | Create | Socket server interface |
| `sim/socket_server.c` | Create | Unix domain socket implementation |
| `sim/cv_source.h` | Create | CV source manager interface |
| `sim/cv_source.c` | Create | LFO, envelope, wavetable implementations |
| `sim/command_handler.h` | Create | JSON command parsing interface |
| `sim/command_handler.c` | Create | Command dispatch logic |
| `sim/sim_main.c` | Modify | Add socket server integration, CV source tick |
| `sim/sim_state.h` | Modify | Add cv_source info to state struct |
| `sim/render/json_renderer.c` | Modify | Include cv_source in JSON output |
| `sim/CMakeLists.txt` | Modify | Add new source files |

## Implementation Phases

### Phase 1: CV Source Manager (no socket)
1. Implement `cv_source.h/c` with manual and LFO modes
2. Integrate into sim main loop
3. Add keyboard shortcuts for quick LFO toggle (e.g., `L` = 1Hz sine LFO)
4. Test LFO behavior, verify timing accuracy

### Phase 2: Socket Server MVP
1. Implement `socket_server.h/c` with basic accept/read/write
2. Add `--socket` CLI flag
3. Implement command handler for button and cv_manual commands
4. Test with `socat` or simple Python client

### Phase 3: Full CV Sources
1. Add envelope generator with ADSR
2. Add wavetable source
3. Add cv_source info to JSON state output
4. Update JSON schema

### Phase 4: Frontend MVP (Python/pygame)
1. Create `tools/sim_frontend.py` (or separate repo)
2. Pygame window with gamepad support
3. Basic LFO controls (frequency slider, shape selector)
4. Button mapping for Xbox controller

### Phase 5: Polish
1. Script commands for CV sources (`.gks` support)
2. Error handling and reconnection
3. Documentation

## Alternatives Considered

### Stream CV samples from frontend
Frontend generates LFO/envelope samples, streams over socket. Rejected because:
- IPC jitter affects timing
- Higher bandwidth
- Less reproducible (depends on frontend performance)
- Harder to script/test

### SDL integration for gamepad
Add SDL2 dependency to simulator for direct gamepad support. Rejected because:
- Heavy dependency for a CLI tool
- Gamepad support varies across platforms
- Socket approach is more flexible (any frontend can connect)

### TCP instead of Unix socket
Would allow remote frontends (e.g., phone app). Deferred because:
- Unix socket is simpler, lower latency
- No port management needed
- Can add TCP listener later if needed

### ZeroMQ/nanomsg
Nice message patterns, but adds external dependency. Plain sockets are sufficient for single-client use case.

## Decisions

1. **Socket vs stdin/stdout** - Support both. Stdin/stdout for simple spawned processes, Unix socket for richer frontends.

2. **State update rate** - Sim ticks at 1ms internally. State pushed to clients at max 60Hz, only when dirty. Server-side rate limiting (not client polling). This matches typical game server architecture.

3. **Wavetable memory** - Dynamic allocation (`malloc`) with 4096 sample max. Free previous buffer on new wavetable load. Sim-only code isn't bound by embedded constraints.

4. **Multiple CV sources** - Future consideration, not in initial scope.

5. **Directory structure**:
   - `tools/sim/gk-sim/` - Simulator backend (C, builds to `gatekeeper-sim`)
   - `tools/sim/gk-sim-frontends/ironclad/` - Socket frontend (Python/pygame, named "Ironclad" - Fe, get it?)

## Open Questions

1. **Stdin/stdout protocol** - Same NDJSON as socket, or simpler line-based commands? Leaning toward same protocol for consistency.

## References

- FDP-010: Simulator Architecture Refactor (current sim architecture)
- `sim/sim_state.h` - Current state aggregation
- `sim/sim_hal.h` - Current CV input via `sim_set_cv_voltage()`
- `sim/schema/sim_state_v1.json` - JSON output schema

---

## Addenda
