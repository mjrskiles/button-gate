# Gatekeeper

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A digital gate/trigger processor for Eurorack modular synthesizers. Press a button (or send a CV signal), get a configurable output: hold it, pulse it, or toggle it.

This project is designed as a reference for those learning synth/modular DIY and embedded development on AVR microcontrollers.

## Features

- **Multiple Modes:**
  - **Gate:** Output stays high while input is held
  - **Pulse:** Rising edge triggers a fixed-duration pulse
  - **Toggle:** Each press flips the output state

- **Persistent Settings:**
  Mode selection is saved to EEPROM and restored on power-up. Factory reset is available by holding both buttons for 3 seconds during startup.

- **Debounced Input:**
  Robust button state detection with rising and falling edge detection.

- **Configuration Action:**
  Change operating modes by tapping the button several times in quick succession and holding on the final tap.

- **Hardware Abstraction Layer (HAL):**
  Clean interface for hardware access, enabling comprehensive unit testing on the host machine.

- **Unit Testing:**
  69 tests using the Unity framework verify functionality without hardware.

## Hardware

**Target:** ATtiny85 @ 1 MHz (8 MHz internal oscillator with CKDIV8 fuse enabled)

| Resource | Size | Usage |
|----------|------|-------|
| Flash | 8 KB | ~3.2 KB used |
| SRAM | 512 B | Stack and globals |
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
| `hardware/` | HAL implementation (pin control, timers, EEPROM) |
| `input/` | Button debouncing and edge detection |
| `output/` | CV output behaviors (gate, pulse, toggle) |
| `controller/` | Coordinates input, output, and LED indicators |
| `state/` | Mode enumeration and transitions |
| `app_init.c` | Startup sequence, settings validation, factory reset |
| `utility/` | Delay routines |

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

The simulator runs the application logic on your host machine with an interactive terminal UI:

```
=== Gatekeeper Simulator ===              Time: 1234 ms

  Output: [ HIGH ]

  Button A: [HELD]    Button B: [ -- ]

  [A] Button A    [B] Button B    [Q] Quit
  [R] Reset time  [F] Fast/Realtime toggle

Event Log:
      0 ms  Simulator started
   1000 ms  App initialized, mode=0
   1200 ms  Button A pressed
   1234 ms  Output -> HIGH
```

Use `--fast` for accelerated testing without real-time delays.

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
