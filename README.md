# Gatekeeper

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A digital gate/trigger processor for Eurorack modular synthesizers. Press a button (or send a CV signal), get a configurable output: hold it, pulse it, or toggle it.

This project is designed as a reference for those learning synth/modular DIY and embedded development on AVR microcontrollers.

## Features

- **Five Operating Modes:**
  - **Gate:** Output stays high while input is held
  - **Trigger:** Rising edge triggers a fixed-duration pulse (1-50ms)
  - **Toggle:** Each press flips the output state
  - **Divide:** Clock divider - outputs every N inputs (2-24)
  - **Cycle:** Internal clock generator (default 80 BPM)

- **Persistent Settings:**
  Mode selection is saved to EEPROM and restored on power-up. Factory reset is available by holding both buttons for 3 seconds during startup.

- **Debounced Input:**
  Robust button state detection with rising and falling edge detection.

- **Configuration Action:**
  Change operating modes by tapping the button several times in quick succession and holding on the final tap.

- **Hardware Abstraction Layer (HAL):**
  Clean interface for hardware access, enabling comprehensive unit testing on the host machine.

- **Unit Testing:**
  159 tests using the Unity framework verify functionality without hardware.

- **x86 Simulator:**
  Interactive terminal UI or headless JSON output for testing without hardware.

## Hardware

**Target:** ATtiny85 @ 1 MHz (8 MHz internal oscillator with CKDIV8 fuse enabled)

| Resource | Size | Usage |
|----------|------|-------|
| Flash | 8 KB | ~65% used |
| SRAM | 512 B | ~22% used |
| EEPROM | 512 B | Settings persistence |

**Pin Assignment:**

| Pin | Function |
|-----|----------|
| PB0 | Neopixel data |
| PB1 | CV output |
| PB2 | Button A |
| PB3 | CV input |
| PB4 | Button B |
| PB5 | RESET |

Output LED is driven directly from the buffered output circuit, not GPIO.

## Code Architecture

The firmware is organized into modules:

| Module | Purpose |
|--------|---------|
| `core/` | Application coordinator - manages FSM hierarchy, routes events |
| `fsm/` | Generic table-driven FSM engine (reusable library) |
| `events/` | Event processor - button gestures, CV edge detection |
| `modes/` | Signal processing modes (Gate, Trigger, Toggle, Divide, Cycle) |
| `hardware/` | HAL implementation (pin control, timers, EEPROM) |
| `input/` | Button debouncing and edge detection |
| `output/` | CV output behaviors |
| `app_init.c` | Startup sequence, settings validation, factory reset |

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for detailed documentation including data flow diagrams, EEPROM layout, and extension guides.

## Build Instructions

### Prerequisites

**Firmware build:**
- AVR-GCC toolchain (`avr-gcc`, `avr-objcopy`, `avr-size`, `avr-strip`)
- CMake 3.16+
- Optional: `avrdude` (flashing), `bc` + `avr-nm` (size analysis)

**Test/Simulator build** (no AVR toolchain needed):
- GCC (host compiler)
- CMake 3.16+

Install on Debian/Ubuntu:
```bash
# Tests only
sudo apt install build-essential cmake

# Firmware (minimal)
sudo apt install gcc-avr avr-libc cmake

# Firmware (full: build + flash + analysis)
sudo apt install gcc-avr avr-libc binutils-avr avrdude cmake bc
```

### Quick Start

```bash
# Build firmware (with size analysis)
mkdir build && cd build
cmake .. && make

# Build firmware (minimal, no analysis dependencies)
cmake -DSIZE_REPORT=OFF .. && make

# Build and run tests
mkdir build_tests && cd build_tests
cmake -DBUILD_TESTS=ON .. && make
./test/unit/gatekeeper_unit_tests

# Build and run x86 simulator
mkdir build_sim && cd build_sim
cmake -DBUILD_SIM=ON .. && make
./sim/gatekeeper-sim
```

The default build runs a post-build size analysis showing flash/RAM usage and largest symbols. If `bc` is not installed, the build will warn and fall back to basic `avr-size` output.

### x86 Simulator

The simulator runs the application logic on your host machine with multiple output modes:

```bash
./sim/gatekeeper-sim              # Interactive terminal UI
./sim/gatekeeper-sim --json       # JSON output (NDJSON format)
./sim/gatekeeper-sim --batch      # Plain text (for scripts/CI)
./sim/gatekeeper-sim --fast       # Fast-forward mode
./sim/gatekeeper-sim --script test.gks  # Run test script
```

**Terminal UI:**
```
=== Gatekeeper Simulator ===              Time: 1234 ms

  LEDs: [███] [███]  (Mode / Activity)

  State: PERFORM     Mode: GATE     Page: --

  Output: [ HIGH ]

  Button A: [HELD]    Button B: [ -- ]

  [A] Button A  [B] Button B  [Q] Quit
  [R] Reset     [F] Fast/Real [L] Legend
```

**JSON Output:**

The `--json` flag outputs state as [NDJSON](https://github.com/ndjson/ndjson-spec) (Newline Delimited JSON) - one JSON object per line, no external dependencies. Schema defined in `sim/schema/sim_state_v1.json`.

```json
{"version":1,"timestamp_ms":1234,"state":{"top":"PERFORM","mode":"GATE","page":null},"inputs":{"button_a":true,"button_b":false,"cv_in":false},"outputs":{"signal":true},"leds":[{"index":0,"name":"mode","r":0,"g":255,"b":0},{"index":1,"name":"activity","r":255,"g":255,"b":255}],"events":[]}
```

This enables piping to `jq`, logging, or building custom frontends.

### Flashing

```bash
make flash        # Program firmware
make fuses        # Set fuse configuration
make read_fuses   # Verify fuses
```

Default programmer: stk500v2 on `/dev/ttyACM0`. Override with:

```bash
cmake -DPROGRAMMER=usbasp -DPROGRAMMER_PORT=/dev/ttyUSB0 ..
```

## License

MIT
