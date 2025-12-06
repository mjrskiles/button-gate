# Button Gate

Button Gate is a firmware project for the ATtiny85 microcontroller that reads a debounced button input and controls a CV (control voltage) output using one of several modes: Gate, Pulse, and Toggle. The firmware supports a configuration action—triggered by a sequence of taps followed by a hold—which cycles through the available operating modes.

## Features

- **Multiple Modes:**  
  - **Gate:** Output remains high as long as the button is pressed.
  - **Pulse:** A fixed-duration high pulse is generated on the rising edge.
  - **Toggle:** Each button press toggles the output state.
- **Debounced Button Input:**  
  Robust button state detection with rising and falling edge detection.
- **Configuration Action:**  
  Change operating modes by tapping the button several times in quick succession and holding on the final tap.
- **Hardware Abstraction Layer (HAL):**  
  Provides a clean interface for managing I/O pins and timer-based delays.
- **Modular Code Architecture:**  
  The project separates concerns into modules for hardware, input, output, state management, and utility functions.
- **Unit Testing:**  
  Integrated unit tests using the Unity framework to verify functionality.

## Code Architecture

The project is organized into several directories:

- **hardware:**  
  Contains the HAL implementation (pin control, timer initialization, and a millisecond counter).

- **input:**  
  Implements button handling with filtering, edge detection, and config action detection.

- **output:**  
  Manages CV output behaviors including gate, pulse, and toggle updates.

- **controller:**  
  Ties the input and output modules together. The IO controller applies the selected mode logic, updates output functions, and drives LED indicators.

- **state:**  
  Manages application modes and facilitates switching between them (e.g., via the configuration action).

- **utility:**  
  Provides helper functions such as delay routines used during the startup sequence.

- **test:**  
  Contains unit tests (using Unity) to verify the functionality of individual modules.

The `main.c` file sets up the HAL, runs a startup sequence (flashing the LEDs), initializes the button and output, and then continuously polls the IO controller to process input and update the output.

## Build Instructions

This project uses CMake for its build system.

### Prerequisites

- AVR-GCC toolchain (avr-gcc, avr-objcopy, avr-size, etc.)
- CMake (version 3.16 or later)
- avrdude (for flashing)
- (Optional) GCC for building and running unit tests with the `TEST_BUILD` option

### Building the Application

1. **Create and enter a build directory:**
   ```bash
   mkdir build && cd build
   ```

2. **Configure the project:**
   ```bash
   cmake ..
   ```

3. **Build the project:**
   ```bash
   make
   ```

This process compiles the firmware for the ATtiny85 and, as a post-build step, strips debug symbols and generates a HEX file.

### Building Unit Tests

To build the unit tests (which run on your host machine and omit AVR-specific code):

1. **Configure with tests enabled:**
   ```bash
   mkdir build_tests && cd build_tests
   cmake -DBUILD_TESTS=ON ..
   ```

2. **Build the tests:**
   ```bash
   make
   ```

3. **Run the tests:**
   ```bash
   ./test/unit/button-gate_unit_tests
   ```

## Flashing the Microcontroller

After building the application, a HEX file is generated from the ELF binary. Use the included flash target to program your ATtiny85.

### Flashing the Application

1. **Connect your ATtiny85 via the appropriate programmer (e.g., stk500v2).**

2. **Flash the firmware:**
   ```bash
   make flash
   ```

   The command uses avrdude with settings defined in the CMakeLists.txt:
   - **MCU:** ATtiny85
   - **Programmer:** stk500v2 (default)
   - **Programmer Port:** `/dev/ttyACM0` (default)

   To use a different programmer or port, configure CMake with:
   ```bash
   cmake -DPROGRAMMER=usbasp -DPROGRAMMER_PORT=/dev/ttyUSB0 ..
   ```

### Fuse Settings

- **Write Fuses:**  
  To program the fuse settings with safe defaults:
  ```bash
  make fuses
  ```

- **Read Fuses:**  
  To read the current fuse settings:
  ```bash
  make read_fuses
  ```

## Debugging & Optimization

- **Stripped Debug Info:**  
  The release build strips debug symbols (via the `-s` flag and `avr-strip`) to reduce binary size.
- **Unit Testing:**  
  The unit tests use a host compiler (gcc) and include a `TEST_BUILD` macro to skip AVR-specific code during test compilation.

## License

MIT
