# ADR-003: Hierarchical FSM and State Management Architecture

## Status

Accepted

## Date

2025-12-08

## Context

Gatekeeper is evolving from a simple 3-mode gate processor to a feature-rich module with:

- 5 operating modes (Gate, Trigger, Toggle, Divide, Cycle)
- A menu system with 8+ settings pages
- Complex button gestures (tap, hold, compound)
- CV input with configurable behavior per mode
- Mode-specific LED feedback

The current implementation uses ad-hoc switch-case logic in `io_controller.c`. This approach doesn't scale—adding modes requires modifying multiple switch statements, and there's no clear separation between state management and signal processing.

We need an architecture that:
1. Explicitly defines all states and transitions
2. Separates state orchestration from mode-specific behavior
3. Supports mode-specific LED feedback
4. Fits within ATtiny85's 512 bytes SRAM / 8KB flash

## Decision

Implement a **hierarchical finite state machine** with **dedicated mode handlers**.

### Three-Layer Architecture

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
           │                                     │
           ▼                                     ▼
┌───────────────────┐                 ┌───────────────────┐
│     Mode FSM      │                 │     Menu FSM      │
│  Gate → Trigger   │                 │   Page ring       │
│  → Toggle → etc   │                 │   navigation      │
└───────────────────┘                 └───────────────────┘
           │
           ▼
┌───────────────────┐
│   Mode Handlers   │
│  (signal + LED)   │
└───────────────────┘
```

### Key Components

**1. Reusable FSM Engine**

Table-driven FSM with transitions stored in PROGMEM:

```c
typedef struct {
    uint8_t from_state;
    uint8_t event;
    uint8_t to_state;
    void (*action)(void);
} Transition;

bool fsm_process_event(FSM *fsm, uint8_t event);
void fsm_update(FSM *fsm);
```

**2. Event Processor**

Transforms raw inputs into semantic events with proper timing:

- Press events fire immediately (performance-critical)
- Release/tap events fire on button up (configuration actions)
- Compound gestures detected (A:hold+B:tap → menu enter)

**3. Mode Handler Interface**

Each mode implements a handler with signal processing AND LED feedback:

```c
typedef struct {
    bool (*process)(void *ctx, bool input, bool *output);
    void (*on_activate)(void *ctx);
    void (*on_deactivate)(void *ctx);
    void (*get_led_feedback)(void *ctx, LEDFeedback *feedback);
} ModeHandler;
```

**4. CV Input with Hysteresis**

ADC-based input with software hysteresis (~0.5V band) for noise immunity on Eurorack CV signals.

### Design Principles

1. **Music never stops**: CV/button processing continues during menu navigation
2. **Hybrid architecture**: FSM orchestrates, handlers process signals
3. **Mode-specific LED feedback**: Each handler knows what its LEDs should show
4. **Menu timeout**: Auto-exit after 20s (configurable 15-30s)

## Consequences

**Benefits:**

- **Predictable behavior**: All states and transitions explicitly defined in tables
- **Testable**: FSM engine, event processor, and handlers testable in isolation
- **Extensible**: Adding modes requires only implementing a handler and adding table entries
- **Memory efficient**: Transition tables in PROGMEM, minimal RAM overhead (~50 bytes)
- **Clean separation**: State management decoupled from signal processing
- **Mode-specific LED**: Natural place for each mode's visual feedback logic

**Costs:**

- **Initial complexity**: More infrastructure than switch-case
- **Learning curve**: Table-driven FSM pattern may be unfamiliar
- **Indirection**: Handler function pointers add small overhead

**Trade-offs:**

- Chose hierarchical FSM over flat (better organization, slightly more code)
- Chose hybrid handlers over FSM-only (cleaner signal processing)
- Chose PROGMEM tables over RAM (saves ~100 bytes RAM, costs flash reads)

## Alternatives Considered

### 1. Extended switch-case (current approach)

Add more cases to existing io_controller.c.

**Rejected**: Doesn't scale. Mode logic becomes tangled. No clear extension points.

### 2. Full statechart library

Use a complete UML statechart implementation with orthogonal regions, history states, etc.

**Rejected**: Overkill for this application. Significant code size overhead.

### 3. Flat table-driven FSM

Single FSM with all states (modes + menu pages) at one level.

**Rejected**: Transition table becomes large and unwieldy. Harder to reason about.

### 4. Mode behavior in FSM state updates

Put signal processing logic directly in FSM state `on_update()` functions.

**Rejected**: Mixes concerns. Harder to test. FSM becomes "fat".

## Open Questions

None—design is fully specified in FDP-004 and FDP-005.

## References

- [FDP-004: State Machine Module](../feature-designs/FDP-004-state-machine.md)
- [FDP-005: Neopixel Driver & LED Feedback](../feature-designs/FDP-005-neopixel-led-feedback.md)
- [ADR-001: Rev2 Architecture](001-rev2-architecture.md)
