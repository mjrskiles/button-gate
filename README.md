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

**Target:** ATtiny85 @ 8 MHz internal oscillator

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

- AVR-GCC toolchain (avr-gcc, avr-objcopy, avr-size)
- CMake 3.16+
- avrdude (for flashing)
- GCC (for unit tests)

### Quick Start

```bash
# Build firmware
mkdir build && cd build
cmake .. && make

# Build and run tests
mkdir build_tests && cd build_tests
cmake -DBUILD_TESTS=ON .. && make
./test/unit/gatekeeper_unit_tests
```

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
