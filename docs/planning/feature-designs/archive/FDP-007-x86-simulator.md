# FDP-007: x86 CLI Simulator

## Status

Proposed

## Summary

Add an interactive x86 CLI simulator that compiles the application logic to the host machine, enabling rapid testing and debugging without hardware. The simulator uses a terminal-based interface where keyboard input simulates buttons and CV, and output state is displayed in real-time.

## Motivation

Hardware-in-the-loop testing is slow and requires physical access to the module. An x86 simulator provides:

1. **Rapid iteration**: Test changes instantly without flash cycles
2. **Debugging**: Use printf, GDB, sanitizers, and valgrind
3. **CI integration**: Run behavioral tests in automated pipelines
4. **Accessibility**: Develop and test without hardware present
5. **Demo/documentation**: Show behavior without physical module

The existing HAL abstraction already enables this - the mock HAL used in unit tests proves the application logic compiles cleanly to x86. The simulator extends this by adding interactive I/O.

## Detailed Design

### Overview

```
┌──────────────────────────────────────────────────────────┐
│                    Terminal UI                           │
│                                                          │
│   Gatekeeper Simulator                    Time: 12345ms  │
│   ─────────────────────────────────────────────────────  │
│                                                          │
│   Mode: GATE        Output: [████] HIGH                  │
│   Button A: [ ]     Button B: [ ]     CV In: [ ]         │
│                                                          │
│   ─────────────────────────────────────────────────────  │
│   [A] Toggle A   [B] Toggle B   [Space] CV pulse         │
│   [M] Next mode  [R] Reset      [Q] Quit                 │
│                                                          │
│   Event log:                                             │
│   > 12340ms: Button A pressed                            │
│   > 12345ms: Output HIGH                                 │
└──────────────────────────────────────────────────────────┘
```

The simulator:
1. Compiles all application code (`src/`) to x86
2. Links against a "sim HAL" instead of mock HAL or real HAL
3. Runs the main loop with real or accelerated time
4. Maps keyboard input to button/CV state
5. Displays output state changes

### Architecture Decision: Separate Main vs Conditional Includes

Two approaches are possible for integrating the simulator:

#### Option A: Separate sim_main.c (Recommended)

```
src/
├── main.c              # AVR entry point (includes <avr/io.h>)
├── app.c               # Application logic (NEW - extracted from main.c)
├── ...
sim/
├── sim_main.c          # Simulator entry point
├── sim_hal.c           # Terminal-based HAL
├── CMakeLists.txt
```

**Pros:**
- Clean separation of concerns
- No conditional compilation in production code
- Simulator can have its own CLI arguments, UI, etc.
- Easy to add features (logging, scripting, record/playback)
- Production `main.c` stays minimal and obvious

**Cons:**
- Small code duplication (init sequence)
- Two entry points to maintain

#### Option B: Conditional Includes in main.c

```c
// main.c
#ifdef SIM_BUILD
    #include "sim/sim_hal.h"
#else
    #include <avr/io.h>
    #include "hardware/hal.h"
#endif

int main(void) {
    #ifdef SIM_BUILD
    sim_init_terminal();
    #endif

    p_hal->init();
    // ... rest of app
}
```

**Pros:**
- Single source of truth for init sequence
- Guaranteed parity between sim and production

**Cons:**
- Pollutes production code with simulator concerns
- `#ifdef` soup grows as simulator features increase
- Harder to add simulator-specific features (CLI args, UI modes)
- Violates single responsibility principle
- Makes main.c harder to read and audit

#### Decision: Option A (Separate Main)

**Rationale:**

1. **Separation of concerns**: The simulator is a development tool, not production code. It shouldn't affect the simplicity of `main.c`.

2. **Extensibility**: Simulator features (time acceleration, event scripting, CI mode) don't belong in production code paths.

3. **Precedent**: This mirrors the test build structure - tests have their own entry point (`unit_tests.c`) that exercises the same modules.

4. **Minimal duplication**: The init sequence is ~10 lines. Extracting `app_run()` or similar keeps it DRY if desired.

5. **Auditability**: Production `main.c` should be trivially readable for hardware review.

### Interface Changes

**New sim HAL interface** (`sim/sim_hal.h`):

```c
#ifndef GK_SIM_HAL_H
#define GK_SIM_HAL_H

#include "hardware/hal_interface.h"

// Initialize simulator (terminal setup, etc.)
void sim_init(void);

// Cleanup simulator (restore terminal)
void sim_cleanup(void);

// Process input and update display (call each tick)
// Returns false if user requested quit
bool sim_update(void);

// Get the sim HAL interface
HalInterface* sim_get_hal(void);

// Configuration
void sim_set_realtime(bool realtime);      // Real-time vs fast-forward
void sim_set_tick_rate(uint32_t hz);       // Ticks per second (default 1000)

#endif
```

**Optional app extraction** (`include/app.h`):

```c
#ifndef GK_APP_H
#define GK_APP_H

#include "app_init.h"
#include "controller/io_controller.h"

// Application context (all state needed for main loop)
typedef struct {
    AppSettings settings;
    Button button;
    CVOutput cv_output;
    IOController io_controller;
} App;

// Initialize application (call once at startup)
AppInitResult app_setup(App *app);

// Run one iteration of main loop (call repeatedly)
void app_tick(App *app);

#endif
```

This extraction is optional but recommended - it makes both `main.c` and `sim_main.c` trivial.

### Implementation Details

#### 1. Sim HAL (`sim/sim_hal.c`)

Extends mock_hal with terminal I/O:

```c
#include "sim_hal.h"
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>

static struct termios orig_termios;
static uint8_t pin_states[8] = {0};
static uint8_t eeprom[512];
static uint32_t sim_time_ms = 0;
static bool realtime_mode = true;

// Non-blocking key check
static int kbhit(void) {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

static int getch(void) {
    int ch;
    if (read(STDIN_FILENO, &ch, 1) == 1) return ch;
    return -1;
}

void sim_init(void) {
    // Set terminal to raw mode
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    // Initialize EEPROM to erased state
    memset(eeprom, 0xFF, sizeof(eeprom));

    // Clear screen and hide cursor
    printf("\033[2J\033[?25l");
}

void sim_cleanup(void) {
    // Restore terminal
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    printf("\033[?25h\n");  // Show cursor
}

bool sim_update(void) {
    // Process keyboard input
    if (kbhit()) {
        int ch = getch();
        switch (ch) {
            case 'a': case 'A':
                pin_states[BUTTON_A_PIN] ^= 1;  // Toggle
                break;
            case 'b': case 'B':
                pin_states[BUTTON_B_PIN] ^= 1;
                break;
            case ' ':
                pin_states[CV_IN_PIN] = 1;  // Momentary
                break;
            case 'q': case 'Q':
                return false;  // Quit
        }
    } else {
        // Release momentary keys
        pin_states[CV_IN_PIN] = 0;
    }

    // Update display
    sim_draw();

    // Advance time
    if (realtime_mode) {
        usleep(1000);  // 1ms tick
    }
    sim_time_ms++;

    return true;
}
```

#### 2. Simulator Main (`sim/sim_main.c`)

```c
#include "sim_hal.h"
#include "app.h"
#include <stdio.h>
#include <signal.h>

static volatile bool running = true;

static void handle_sigint(int sig) {
    (void)sig;
    running = false;
}

int main(int argc, char **argv) {
    // Parse args (--fast, --help, etc.)
    bool realtime = true;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--fast") == 0) realtime = false;
        if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: gatekeeper-sim [--fast]\n");
            return 0;
        }
    }

    // Setup
    signal(SIGINT, handle_sigint);
    sim_init();
    sim_set_realtime(realtime);

    p_hal = sim_get_hal();

    App app;
    app_setup(&app);

    // Main loop
    while (running && sim_update()) {
        app_tick(&app);
    }

    sim_cleanup();
    return 0;
}
```

#### 3. Build Integration (`sim/CMakeLists.txt`)

```cmake
# Simulator build (x86)
project(gatekeeper-sim C)

# Collect application sources (exclude main.c)
file(GLOB_RECURSE APP_SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.c"
)
list(FILTER APP_SOURCES EXCLUDE REGEX "main\\.c$")

# Simulator sources
set(SIM_SOURCES
    sim_main.c
    sim_hal.c
)

add_executable(gatekeeper-sim
    ${SIM_SOURCES}
    ${APP_SOURCES}
)

target_include_directories(gatekeeper-sim PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(gatekeeper-sim PRIVATE SIM_BUILD)
target_compile_options(gatekeeper-sim PRIVATE -Wall -Wextra -g -fsanitize=address)
target_link_options(gatekeeper-sim PRIVATE -fsanitize=address)
```

#### 4. Top-level CMake Integration

Add to root `CMakeLists.txt`:

```cmake
option(BUILD_SIM "Build x86 simulator" OFF)

if(BUILD_SIM)
    add_subdirectory(sim)
endif()
```

### Data Structures

**Display state** (internal to sim_hal.c):

```c
typedef struct {
    uint32_t time_ms;
    uint8_t mode;
    bool output_high;
    bool button_a;
    bool button_b;
    bool cv_in;
    char event_log[8][64];  // Rolling log of last 8 events
    int log_head;
} SimDisplay;
```

### Error Handling

| Condition | Handling |
|-----------|----------|
| Terminal not a TTY | Fall back to non-interactive mode (CI) |
| SIGINT received | Clean shutdown, restore terminal |
| Invalid key | Ignore |

### Testing Strategy

1. **Manual testing**: Interactive verification of all modes
2. **CI smoke test**: Run simulator in non-interactive mode with scripted input
3. **Sanitizers**: Build with ASan/UBSan to catch memory bugs

Future: Event scripting for automated behavioral tests.

## File Changes

| File | Change | Description |
|------|--------|-------------|
| `sim/sim_hal.h` | Create | Simulator HAL interface |
| `sim/sim_hal.c` | Create | Terminal-based HAL implementation |
| `sim/sim_main.c` | Create | Simulator entry point |
| `sim/CMakeLists.txt` | Create | Simulator build config |
| `CMakeLists.txt` | Modify | Add BUILD_SIM option |
| `include/app.h` | Create | Optional: extracted app interface |
| `src/app.c` | Create | Optional: extracted app logic |
| `src/main.c` | Modify | Optional: use app.h if extracted |
| `README.md` | Modify | Document simulator build/usage |

## Dependencies

- POSIX terminal APIs (`termios.h`, `select()`)
- No external libraries required

Optional:
- ncurses (for richer UI - not in initial implementation)
- Address sanitizer (recommended for debug builds)

## Alternatives Considered

### 1. simavr (AVR emulator)

Run actual AVR binary in cycle-accurate emulator.

```bash
simavr -m attiny85 -f 8000000 gatekeeper.elf
```

**Rejected for primary use**: Slower iteration, harder to instrument, requires VCD viewer for signal inspection. Good for catching AVR-specific bugs but overkill for logic testing.

**May add later** as complementary tool.

### 2. Conditional compilation in main.c

Use `#ifdef SIM_BUILD` to include simulator code in main.c.

**Rejected**: Pollutes production code, violates separation of concerns, harder to extend. See Architecture Decision section above.

### 3. QEMU user-mode emulation

QEMU can run AVR binaries on x86.

**Rejected**: Complex setup, limited AVR support, no real advantage over simavr for this use case.

### 4. Wokwi / Online simulators

Browser-based ATtiny85 simulation.

**Rejected for primary use**: Limited control, can't integrate with local toolchain, no CI support. Useful for quick demos only.

## Open Questions

1. **ncurses vs raw terminal**: Start with raw ANSI escape codes (no dependency), add ncurses later if needed?

2. **EEPROM persistence**: Should simulator save/load EEPROM state to a file between runs?

3. **Event scripting**: Add support for scripted input sequences for automated testing? (e.g., `sim --script test.txt`)

4. **Time acceleration**: What's the right fast-forward speed? 10x? 100x? Unlimited?

## Implementation Checklist

- [ ] Create `sim/` directory structure
- [ ] Implement sim_hal.c with terminal I/O
- [ ] Implement sim_main.c entry point
- [ ] Add CMake BUILD_SIM option
- [ ] Extract app.h/app.c (optional, decide during implementation)
- [ ] Test all modes interactively
- [ ] Add --fast mode for accelerated testing
- [ ] Update README with simulator instructions
- [ ] Add CI smoke test (optional, future)
