# Architecture

This document describes the firmware architecture for Button Gate, a
Eurorack utility module built on the ATtiny85 microcontroller.

## Overview

Button Gate is a digital gate/trigger processor. It reads a button or CV
input and produces a 5V digital output. Multiple operating modes transform
the input signal in different ways.

```
                    ┌─────────────────┐
  Button/CV ──────▶ │                 │ ──────▶ CV Output (5V)
                    │   ATtiny85      │
                    │                 │ ──────▶ LED Indicators
                    └─────────────────┘
```

## Hardware

### Rev1 (Current)

**Target**: ATtiny85 @ 1 MHz internal oscillator

| Pin | Port | Function        |
|-----|------|-----------------|
| PB0 | 5    | Button input    |
| PB1 | 6    | CV output       |
| PB2 | 7    | Mode LED (top)  |
| PB3 | 2    | Output LED      |
| PB4 | 3    | Mode LED (bottom) |
| PB5 | 1    | RESET           |

Mode LEDs use 2-bit binary encoding: Gate=10, Pulse=01, Toggle=11.

### Rev2 (Planned)

See [ADR-001](adr/001-rev2-architecture.md) for rationale.

**Target**: ATtiny85 @ 8 MHz internal oscillator

| Pin | Port | Function        |
|-----|------|-----------------|
| PB0 | 5    | Neopixel data   |
| PB1 | 6    | CV output       |
| PB2 | 7    | Button A        |
| PB3 | 2    | CV input        |
| PB4 | 3    | Button B        |
| PB5 | 1    | RESET           |

Output LED driven directly from buffered output circuit, not GPIO.

## Module Descriptions

### HAL (Hardware Abstraction Layer)

Location: `src/hardware/hal.c`, `include/hardware/hal_interface.h`

The HAL provides a swappable interface for hardware access. A global
pointer `p_hal` references the active implementation.

```c
typedef struct {
    uint8_t button_pin;
    uint8_t sig_out_pin;
    // ... other pins

    void (*init)(void);
    void (*set_pin)(uint8_t pin);
    void (*clear_pin)(uint8_t pin);
    void (*toggle_pin)(uint8_t pin);
    uint8_t (*read_pin)(uint8_t pin);
    uint32_t (*millis)(void);
    // ... other functions
} HalInterface;

extern HalInterface *p_hal;
```

Production code uses the real HAL. Tests swap in a mock HAL with virtual
pins and controllable time.

**Timer**: Timer0 runs in CTC mode with prescaler 8, generating a 1ms
interrupt. The `millis()` function returns elapsed time since startup.

### Button

Location: `src/input/button.c`

Handles debouncing and edge detection for button input.

**State**:
- `pressed`: Debounced button state
- `rising_edge`: True for one cycle after press
- `falling_edge`: True for one cycle after release
- `config_action`: True when config gesture detected

**Config Action Detection** (Rev1):
1. Count rising edges within 500ms windows
2. After 5 taps, start hold timer
3. If held for 1000ms, set `config_action` flag

**Timing Constants**:
```c
#define EDGE_DEBOUNCE_MS  5
#define TAP_TIMEOUT_MS    500
#define TAPS_TO_CHANGE    5
#define HOLD_TIME_MS      1000
```

### CV Output

Location: `src/output/cv_output.c`

Manages the output pin based on current mode and input state.

**Modes**:

| Mode   | Behavior |
|--------|----------|
| Gate   | Output follows input directly |
| Pulse  | Rising edge triggers 10ms pulse |
| Toggle | Rising edge flips output state |

Each mode tracks `last_input_state` to detect edges internally.

### Mode

Location: `src/state/mode.c`

Defines the mode enumeration and helper functions.

```c
typedef enum {
    MODE_GATE = 0,
    MODE_PULSE = 1,
    MODE_TOGGLE = 2,
} CVMode;
```

`cv_mode_get_next()` returns the next mode in cycle.
`cv_mode_get_led_state()` returns LED configuration for the mode.

### IO Controller

Location: `src/controller/io_controller.c`

Coordinates input, output, and state modules. This is the main
application logic.

**Update Loop**:
1. Poll button state
2. Check for config action, advance mode if triggered
3. Apply input filter (`ignore_pressed` during mode change)
4. Call mode-specific output update
5. Update LED indicators

The `ignore_pressed` flag prevents the button hold (during config action)
from affecting the output.

## Data Flow

```
Button Pin (PB0)
      │
      ▼
┌─────────────┐
│   Button    │ ─── config_action ───▶ Mode advance
│  Debounce   │
└─────────────┘
      │
      │ pressed, rising_edge, falling_edge
      ▼
┌─────────────┐
│     IO      │
│ Controller  │ ─── mode ───▶ LED indicators
└─────────────┘
      │
      │ input_triggered (filtered)
      ▼
┌─────────────┐
│  CV Output  │
│  (per mode) │
└─────────────┘
      │
      ▼
Output Pin (PB1)
```

## Testing

Tests run on the host machine using the Unity framework. The mock HAL
provides:

- Virtual pin state array (read/write any pin)
- Virtual millisecond timer (`advance_mock_time()`)
- Deterministic behavior for timing-dependent tests

**Running tests**:
```sh
mkdir build_tests && cd build_tests
cmake -DBUILD_TESTS=ON ..
cmake --build .
./test/unit/button-gate_unit_tests
```

**Test organization**: Each module has a corresponding test file in
`test/unit/`. Tests cover edge cases, timing boundaries, and integration
between modules.

## Build System

CMake-based build with two configurations:

**Firmware build** (default):
```sh
mkdir build && cd build
cmake ..
cmake --build .
```

Produces `button-gate.hex` for flashing.

**Test build**:
```sh
cmake -DBUILD_TESTS=ON ..
```

Compiles with host GCC, defines `TEST_BUILD` macro, links Unity.

**Flashing**:
```sh
make flash           # Program hex to device
make fuses           # Set fuse configuration
make read_fuses      # Verify fuses
```

Default programmer: stk500v2 on /dev/ttyACM0.

## Memory Constraints

The ATtiny85 has limited resources:

| Resource | Size   | Notes |
|----------|--------|-------|
| Flash    | 8 KB   | Program code |
| SRAM     | 512 B  | Stack and globals |
| EEPROM   | 512 B  | Persistent storage |

**Strategies**:
- Global HAL pointer avoids passing pointers through call stack
- `-Os` optimization for code size
- `-fshort-enums` for 1-byte enums
- No dynamic allocation
- Transition tables in PROGMEM (flash) when using state machines

## Extending the Firmware

### Adding a New Mode

1. Add enum value to `CVMode` in `include/state/mode.h`
2. Update `cv_mode_get_next()` in `src/state/mode.c`
3. Update `cv_mode_get_led_state()` for LED encoding
4. Add update function in `src/output/cv_output.c`
5. Add case in `io_controller_update()` switch statement
6. Add tests in `test/unit/output/test_cv_output.h`

### Adding a New HAL Function

1. Add function pointer to `HalInterface` struct
2. Implement in `src/hardware/hal.c` (production)
3. Implement in `test/unit/mocks/mock_hal.c` (test)
4. Initialize pointer in both HAL instances

## Architecture Decisions

Design decisions are documented in `docs/adr/`:

- [ADR-001](adr/001-rev2-architecture.md): Rev2 hardware and firmware changes

## References

- [ATtiny85 Datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-2586-AVR-8-bit-Microcontroller-ATtiny25-ATtiny45-ATtiny85_Datasheet.pdf)
- [AVR Libc Reference](https://www.nongnu.org/avr-libc/user-manual/)
- [Unity Test Framework](https://github.com/ThrowTheSwitch/Unity)
