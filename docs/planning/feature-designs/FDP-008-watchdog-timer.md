# FDP-008: Watchdog Timer Support

## Status

Draft

## Summary

Add watchdog timer (WDT) support to automatically reset the device if the main loop stalls. This provides a safety net against software hangs, ensuring the module remains responsive in a live performance context.

## Motivation

Eurorack modules must be reliable during live performance. If the firmware hangs due to an unforeseen edge case, the module becomes unresponsive with no recovery path except power cycling the entire rack.

The ATtiny85's built-in watchdog timer provides hardware-level recovery:
- If software fails to "pet" the watchdog within the timeout period, the MCU resets
- Recovery happens automatically without user intervention
- Timeout periods from 16ms to 8 seconds are available

Klaus audit recommended adding watchdog support as a reliability improvement.

## Detailed Design

### Overview

The watchdog will be:
1. **Disabled early in startup** (ATtiny85 keeps WDT enabled after reset)
2. **Enabled after initialization** with a 1-second timeout
3. **Petted once per main loop iteration**

This ensures the device recovers from hangs while allowing adequate time for each loop iteration.

### Interface Changes

Add watchdog functions to the HAL interface:

```c
// In hal_interface.h
typedef struct HalInterface {
    // ... existing fields ...

    // Watchdog functions
    void     (*wdt_enable)(uint8_t timeout);   // Enable with timeout constant
    void     (*wdt_disable)(void);             // Disable watchdog
    void     (*wdt_reset)(void);               // Pet the watchdog
} HalInterface;

// Timeout constants (mirror avr-libc WDTO_* values)
#define GK_WDTO_16MS   0
#define GK_WDTO_32MS   1
#define GK_WDTO_64MS   2
#define GK_WDTO_125MS  3
#define GK_WDTO_250MS  4
#define GK_WDTO_500MS  5
#define GK_WDTO_1S     6
#define GK_WDTO_2S     7
#define GK_WDTO_4S     8
#define GK_WDTO_8S     9
```

### Implementation Details

#### 1. Early Disable (Critical)

The ATtiny85 keeps the watchdog running after a WDT-triggered reset, using the fastest prescaler (~16ms). If not disabled immediately, the device will reset-loop.

In `hal.c`, the init function must:
```c
#include <avr/wdt.h>

// MUST be called very early - before any other initialization
void hal_wdt_early_disable(void) __attribute__((naked, section(".init3")));
void hal_wdt_early_disable(void) {
    MCUSR = 0;      // Clear reset flags (including WDRF)
    wdt_disable();  // Disable watchdog
}
```

Using `.init3` section ensures this runs before `main()` or any constructors.

#### 2. HAL Implementation (AVR)

```c
// In hal.c
#include <avr/wdt.h>

static void hal_wdt_enable(uint8_t timeout) {
    wdt_enable(timeout);
}

static void hal_wdt_disable(void) {
    wdt_disable();
}

static void hal_wdt_reset(void) {
    wdt_reset();
}
```

#### 3. Mock HAL Implementation

```c
// In mock_hal.c
static uint8_t mock_wdt_enabled = 0;
static uint8_t mock_wdt_timeout = 0;
static uint32_t mock_wdt_last_reset = 0;

static void mock_wdt_enable(uint8_t timeout) {
    mock_wdt_enabled = 1;
    mock_wdt_timeout = timeout;
    mock_wdt_last_reset = mock_time;
}

static void mock_wdt_disable(void) {
    mock_wdt_enabled = 0;
}

static void mock_wdt_reset(void) {
    mock_wdt_last_reset = mock_time;
}

// Test helper: check if WDT would have triggered
bool mock_wdt_would_trigger(void);
```

#### 4. Simulator HAL Implementation

```c
// In sim_hal.c - WDT is essentially no-op for simulator
static void sim_wdt_enable(uint8_t timeout) {
    (void)timeout;  // Log if verbose mode
}

static void sim_wdt_disable(void) {}

static void sim_wdt_reset(void) {}
```

#### 5. Main Loop Integration

```c
// In main.c
int main(void) {
    // HAL init already disabled WDT via .init3

    p_hal->init();
    app_init_run(&settings);
    coordinator_init(&coordinator, &settings);
    coordinator_start(&coordinator);

    // Enable watchdog with 1-second timeout
    p_hal->wdt_enable(GK_WDTO_1S);

    while (1) {
        coordinator_update(&coordinator);

        // Output handling...

        // Pet watchdog at end of each loop
        p_hal->wdt_reset();
    }
}
```

### Timeout Selection

| Timeout | Use Case |
|---------|----------|
| 16-64ms | Too aggressive - normal operation may exceed |
| 125-250ms | Tight but viable if loop is fast |
| 500ms | Conservative for most embedded apps |
| **1 second** | **Recommended** - safe margin, quick recovery |
| 2-8 seconds | Too slow - noticeable outage |

The 1-second timeout provides:
- Ample margin for worst-case loop iteration
- Quick recovery (audience unlikely to notice)
- Standard choice for embedded systems

### Edge Cases

1. **Factory reset hold**: The 3-second button hold is within WDT timeout if we pet during the wait loop. Add `wdt_reset()` call in `reset_feedback_tick()`.

2. **Blocking delays**: `util_delay_ms()` with long delays (>1s) would trigger WDT. Current code uses short delays (max 500ms). If longer delays are needed, they must pet WDT periodically.

3. **EEPROM writes**: EEPROM operations are slow but well under 1 second. No special handling needed.

### Data Structures

No new data structures required. The HAL interface gains three function pointers.

### Error Handling

- If WDT triggers, device resets and boots normally
- No explicit error signaling (reset is the recovery)
- MCUSR can be checked at boot to detect WDT reset (optional logging)

### Testing Strategy

1. **Unit tests** (mock HAL):
   - Verify `wdt_enable()` sets mock state
   - Verify `wdt_reset()` updates last-reset timestamp
   - Verify `mock_wdt_would_trigger()` returns true if timeout exceeded

2. **Simulator**:
   - WDT functions are no-ops (simulator doesn't need crash recovery)
   - Optional: log WDT pets in verbose mode

3. **Hardware testing**:
   - Add intentional infinite loop, verify device resets
   - Verify normal operation doesn't trigger WDT
   - Test factory reset sequence completes without WDT interference

## File Changes

| File | Change | Description |
|------|--------|-------------|
| `include/hardware/hal_interface.h` | Modify | Add WDT function pointers and timeout constants |
| `src/hardware/hal.c` | Modify | Implement WDT functions, add early-disable in .init3 |
| `test/unit/mocks/mock_hal.c` | Modify | Implement mock WDT with test helpers |
| `test/unit/mocks/mock_hal.h` | Modify | Declare mock WDT test helpers |
| `sim/sim_hal.c` | Modify | Add no-op WDT implementations |
| `src/main.c` | Modify | Enable WDT after init, pet in main loop |
| `src/app_init.c` | Modify | Add WDT pet in factory reset wait loop |
| `test/unit/app_init/test_app_init.h` | Modify | Add WDT-related test cases |

## Dependencies

- `<avr/wdt.h>` - AVR-LibC watchdog macros (already available in toolchain)
- No external dependencies

## Alternatives Considered

### 1. Software Watchdog

Implement watchdog purely in software using Timer interrupts.

**Rejected**: Hardware WDT is more reliable - it works even if timer ISR is stuck.

### 2. WDT with Interrupt Mode

ATtiny85 supports combined WDT+ISR mode where an interrupt fires before reset.

**Rejected**: Adds complexity (ISR handler, state saving) for minimal benefit. Simple reset is sufficient for this application.

### 3. Longer Timeout (4-8 seconds)

More conservative, less risk of false triggers.

**Rejected**: User-noticeable delay before recovery in a performance context.

### 4. No Watchdog

Rely on software correctness.

**Rejected**: Defensive programming best practice is defense in depth. WDT is cheap insurance.

## Open Questions

1. **Should we indicate WDT reset at boot?** Could briefly flash LEDs differently if MCUSR shows WDT reset. Low priority - adds code size for minimal benefit.

2. **Should WDT be configurable?** Could store timeout preference in EEPROM. Probably over-engineering for this application.

## Implementation Checklist

- [ ] Add WDT function pointers to `hal_interface.h`
- [ ] Add timeout constants to `hal_interface.h`
- [ ] Implement early-disable in `hal.c` (.init3 section)
- [ ] Implement `hal_wdt_enable/disable/reset` in `hal.c`
- [ ] Implement mock WDT in `mock_hal.c`
- [ ] Add mock test helpers (`mock_wdt_would_trigger()`)
- [ ] Implement no-op WDT in `sim_hal.c`
- [ ] Enable WDT in `main.c` after initialization
- [ ] Add WDT pet to main loop
- [ ] Add WDT pet to factory reset wait loop
- [ ] Write unit tests for WDT mock behavior
- [ ] Hardware test: verify recovery from intentional hang
- [ ] Hardware test: verify normal operation stability
- [ ] Update CLAUDE.md with WDT notes

## References

- [AVR-LibC Watchdog Documentation](https://avrdudes.github.io/avr-libc/avr-libc-user-manual-2.3.0/group__avr__watchdog.html)
- [ATtiny85 Datasheet - Watchdog Timer section](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-2586-AVR-8-bit-Microcontroller-ATtiny25-ATtiny45-ATtiny85_Datasheet.pdf)
- [ATtiny85 WDT with Sleep Mode](https://www.instructables.com/ATtiny85-Watchdog-reboot-Together-With-SLEEP-Andor/)
