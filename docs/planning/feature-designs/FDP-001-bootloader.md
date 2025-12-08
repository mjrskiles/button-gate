# FDP-001: Application Bootloader

## Status

Proposed

## Summary

Replace the current `startup` module with a comprehensive `bootloader` module that handles factory reset detection, EEPROM settings validation, and graceful degradation on errors. This provides a robust initialization sequence suitable for a reference open-source project.

## Motivation

The current startup sequence (`startup.c`) only flashes LEDs as a visual test. For Rev2 with persistent EEPROM settings, we need a proper bootloader that:

1. **Factory Reset**: Allows users to reset to defaults without reprogramming
2. **Settings Validation**: Ensures corrupted EEPROM doesn't cause undefined behavior
3. **Graceful Degradation**: Falls back to safe defaults rather than failing silently
4. **Educational Value**: Demonstrates embedded best practices for newcomers

Additionally, renaming `startup` to `bootloader` better communicates the module's purpose to less experienced users.

## Detailed Design

### Overview

The bootloader executes a deterministic sequence on power-up:

```
Power On
    │
    ▼
┌─────────────┐
│  HAL Init   │  (already in main.c)
└─────────────┘
    │
    ▼
┌─────────────┐
│  LED Test   │  Sequential flash (existing behavior)
└─────────────┘
    │
    ▼
┌─────────────────────┐     Held 3s    ┌─────────────────┐
│ Check Reset Signal  │ ──────────────▶│  Factory Reset  │
│ (Both buttons held) │                 │  Clear EEPROM   │
└─────────────────────┘                 └─────────────────┘
    │ Not held                                  │
    ▼                                           │
┌─────────────┐                                 │
│ Load EEPROM │◀────────────────────────────────┘
└─────────────┘
    │
    ▼
┌─────────────────────┐     Invalid    ┌─────────────────┐
│ Validate Settings   │ ──────────────▶│  Use Defaults   │
│ (magic, checksum,   │                 │  (warn via LED) │
│  range checks)      │                 └─────────────────┘
└─────────────────────┘                         │
    │ Valid                                     │
    ▼                                           │
┌─────────────┐◀────────────────────────────────┘
│  RUN Mode   │
└─────────────┘
```

### Interface Changes

**New public interface** (`include/bootloader.h`):

```c
#ifndef BUTTONGATE_BOOTLOADER_H
#define BUTTONGATE_BOOTLOADER_H

#include <stdbool.h>
#include "hardware/hal_interface.h"
#include "state/mode.h"

// Timing constants
#define BOOTLOADER_LED_DURATION_MS      200   // LED test flash duration
#define BOOTLOADER_RESET_HOLD_MS       3000   // Hold time for factory reset
#define BOOTLOADER_RESET_POLL_MS         50   // Polling interval during reset check

// Boot result codes
typedef enum {
    BOOT_OK,                    // Normal boot, settings loaded
    BOOT_OK_DEFAULTS,           // Booted with defaults (EEPROM invalid/empty)
    BOOT_OK_FACTORY_RESET,      // Factory reset performed
} BootResult;

// Loaded settings (populated by bootloader)
typedef struct {
    CVMode mode;
    uint8_t cv_function;        // CV input function (Rev2)
    uint8_t param1;             // Mode-specific parameter 1
    uint8_t param2;             // Mode-specific parameter 2
} BootSettings;

/**
 * Execute the boot sequence.
 *
 * @param settings  Pointer to struct that will be populated with loaded settings
 * @return          Boot result code indicating how boot completed
 */
BootResult bootloader_run(BootSettings *settings);

/**
 * Get default settings.
 * Used for factory reset and when EEPROM is invalid.
 */
void bootloader_get_defaults(BootSettings *settings);

#endif // BUTTONGATE_BOOTLOADER_H
```

### Implementation Details

#### 1. LED Test Sequence (unchanged behavior)

Flash each LED in sequence to verify hardware:

```c
static void bootloader_led_test(void) {
    // Flash mode LEDs and output indicator in sequence
    // Same as current startup.c implementation
}
```

#### 2. Factory Reset Detection

Check if both buttons are held during boot:

```c
static bool bootloader_check_factory_reset(void) {
    // If buttons not both pressed, return immediately
    if (!p_hal->read_pin(p_hal->button_a_pin) ||
        !p_hal->read_pin(p_hal->button_b_pin)) {
        return false;
    }

    // Both pressed - start counting with visual feedback
    uint32_t held_time = 0;
    while (held_time < BOOTLOADER_RESET_HOLD_MS) {
        // Toggle LEDs for "reset pending" feedback
        p_hal->toggle_pin(p_hal->led_mode_top_pin);
        p_hal->toggle_pin(p_hal->led_mode_bottom_pin);

        util_delay_ms(BOOTLOADER_RESET_POLL_MS);
        held_time += BOOTLOADER_RESET_POLL_MS;

        // If either button released, abort
        if (!p_hal->read_pin(p_hal->button_a_pin) ||
            !p_hal->read_pin(p_hal->button_b_pin)) {
            // Turn off LEDs and return
            p_hal->clear_pin(p_hal->led_mode_top_pin);
            p_hal->clear_pin(p_hal->led_mode_bottom_pin);
            return false;
        }
    }

    // Held long enough - confirm with solid LEDs
    p_hal->set_pin(p_hal->led_mode_top_pin);
    p_hal->set_pin(p_hal->led_mode_bottom_pin);
    p_hal->set_pin(p_hal->led_output_indicator_pin);
    util_delay_ms(500);

    return true;
}
```

**Why both buttons?** Single-button reset risks accidental triggers. Requiring both buttons is a common embedded convention (similar to "hold power + volume" on phones).

**Why 3 seconds?** Long enough to be intentional, short enough to not be frustrating. This is the de facto standard hold duration for factory resets.

#### 3. EEPROM Structure

```c
// EEPROM layout
#define EEPROM_MAGIC_ADDR       0x00    // 2 bytes
#define EEPROM_SETTINGS_ADDR    0x02    // sizeof(EepromSettings)
#define EEPROM_CHECKSUM_ADDR    0x10    // 1 byte (after settings)

#define EEPROM_MAGIC_VALUE      0xBG01  // "BG" + version 01

typedef struct {
    uint8_t mode;           // CVMode enum value
    uint8_t cv_function;    // CV input function
    uint8_t param1;         // Mode-specific
    uint8_t param2;         // Mode-specific
    uint8_t reserved[4];    // Future expansion
} EepromSettings;
```

#### 4. Settings Validation

Three-level validation:

```c
static bool bootloader_validate_eeprom(BootSettings *settings) {
    // Level 1: Magic number (detects uninitialized EEPROM)
    uint16_t magic = eeprom_read_word((uint16_t*)EEPROM_MAGIC_ADDR);
    if (magic != EEPROM_MAGIC_VALUE) {
        return false;
    }

    // Level 2: Checksum (detects corruption)
    EepromSettings stored;
    eeprom_read_block(&stored, (void*)EEPROM_SETTINGS_ADDR, sizeof(stored));
    uint8_t checksum = eeprom_read_byte((uint8_t*)EEPROM_CHECKSUM_ADDR);
    if (checksum != calculate_checksum(&stored)) {
        return false;
    }

    // Level 3: Range validation (detects invalid values)
    if (stored.mode > MODE_TOGGLE) {  // Adjust for Rev2 modes
        return false;
    }

    // All checks passed - copy to output
    settings->mode = stored.mode;
    settings->cv_function = stored.cv_function;
    settings->param1 = stored.param1;
    settings->param2 = stored.param2;

    return true;
}
```

#### 5. Failure Handling (Graceful Degradation)

The bootloader follows the principle of **"fail safe, not fail silent"**:

| Condition | Action | LED Feedback |
|-----------|--------|--------------|
| EEPROM uninitialized | Use defaults, continue to RUN | Double-blink pattern |
| EEPROM corrupted | Use defaults, continue to RUN | Double-blink pattern |
| Invalid values | Use defaults, continue to RUN | Double-blink pattern |

**Why continue rather than halt?** For a Eurorack module:
- Halting makes the module useless until reprogrammed
- Using defaults allows the user to continue making music
- The LED warning alerts them to reconfigure settings

```c
static void bootloader_warn_using_defaults(void) {
    // Double-blink pattern on output LED
    for (int i = 0; i < 2; i++) {
        p_hal->set_pin(p_hal->led_output_indicator_pin);
        util_delay_ms(100);
        p_hal->clear_pin(p_hal->led_output_indicator_pin);
        util_delay_ms(100);
    }
    util_delay_ms(300);
    // Repeat once more
    for (int i = 0; i < 2; i++) {
        p_hal->set_pin(p_hal->led_output_indicator_pin);
        util_delay_ms(100);
        p_hal->clear_pin(p_hal->led_output_indicator_pin);
        util_delay_ms(100);
    }
}
```

#### 6. Main Boot Sequence

```c
BootResult bootloader_run(BootSettings *settings) {
    // 1. LED test (visual hardware check)
    bootloader_led_test();

    // 2. Check for factory reset
    if (bootloader_check_factory_reset()) {
        bootloader_clear_eeprom();
        bootloader_get_defaults(settings);
        bootloader_save_settings(settings);  // Write defaults to EEPROM
        return BOOT_OK_FACTORY_RESET;
    }

    // 3. Load and validate EEPROM
    if (bootloader_validate_eeprom(settings)) {
        return BOOT_OK;
    }

    // 4. Validation failed - use defaults
    bootloader_get_defaults(settings);
    bootloader_warn_using_defaults();
    return BOOT_OK_DEFAULTS;
}
```

### Data Structures

```c
// Boot result (see Interface Changes above)
typedef enum {
    BOOT_OK,
    BOOT_OK_DEFAULTS,
    BOOT_OK_FACTORY_RESET,
} BootResult;

// Settings structure (see Interface Changes above)
typedef struct {
    CVMode mode;
    uint8_t cv_function;
    uint8_t param1;
    uint8_t param2;
} BootSettings;

// Default values
static const BootSettings DEFAULT_SETTINGS = {
    .mode = MODE_GATE,
    .cv_function = 0,       // Gate input (default)
    .param1 = 0,
    .param2 = 0,
};
```

### Error Handling

| Error | Detection | Response |
|-------|-----------|----------|
| Uninitialized EEPROM | Magic number mismatch | Use defaults, warn |
| Corrupted EEPROM | Checksum mismatch | Use defaults, warn |
| Invalid mode value | Range check | Use defaults, warn |
| Button read failure | N/A (no way to detect) | Proceeds normally |

### Testing Strategy

**Unit Tests** (mock HAL):

1. `test_bootloader_led_test` - Verify LED sequence
2. `test_bootloader_factory_reset_detected` - Both buttons held triggers reset
3. `test_bootloader_factory_reset_aborted` - Button release aborts reset
4. `test_bootloader_valid_eeprom` - Valid settings loaded correctly
5. `test_bootloader_invalid_magic` - Missing magic triggers defaults
6. `test_bootloader_invalid_checksum` - Corruption triggers defaults
7. `test_bootloader_invalid_mode` - Out-of-range mode triggers defaults
8. `test_bootloader_defaults_values` - Default values are correct

**Mock EEPROM**: The mock HAL will need EEPROM simulation:

```c
// Add to mock_hal.h
void mock_eeprom_write(uint16_t addr, uint8_t *data, size_t len);
void mock_eeprom_read(uint16_t addr, uint8_t *data, size_t len);
void mock_eeprom_clear(void);
```

**Integration Tests** (on hardware):

1. Fresh chip boots with defaults
2. Settings persist across power cycle
3. Factory reset clears settings
4. Corrupted EEPROM (manual write) recovers gracefully

## File Changes

| File | Change | Description |
|------|--------|-------------|
| `src/startup.c` | Delete | Replaced by bootloader |
| `include/startup.h` | Delete | Replaced by bootloader |
| `src/bootloader.c` | Create | New bootloader implementation |
| `include/bootloader.h` | Create | New bootloader interface |
| `src/main.c` | Modify | Update include, use `bootloader_run()` |
| `src/CMakeLists.txt` | Modify | Update source file list |
| `test/unit/bootloader/test_bootloader.h` | Create | Bootloader unit tests |
| `test/unit/mocks/mock_hal.h` | Modify | Add EEPROM simulation |
| `test/unit/mocks/mock_hal.c` | Modify | Implement EEPROM simulation |
| `test/unit/unit_tests.c` | Modify | Register bootloader tests |
| `docs/ARCHITECTURE.md` | Modify | Update module descriptions |

## Dependencies

- AVR EEPROM library (`<avr/eeprom.h>`) - already available
- Rev2 HAL changes (two button pins) - can stub for Rev1

## Alternatives Considered

### 1. Halt on EEPROM Error

**Rejected**: Makes the module unusable until reprogrammed. Poor UX for a music device where the show must go on.

### 2. Single Button Factory Reset

**Rejected**: Too easy to trigger accidentally. Two-button hold is the industry standard for destructive operations.

### 3. Reset via Menu Only

**Rejected**: If settings are corrupted, menu might not work properly. Hardware reset must be available.

### 4. CRC-16 Instead of Simple Checksum

**Considered**: Better error detection but uses more code space. Simple XOR checksum sufficient for our 8-byte settings block.

### 5. Keep "startup" Name

**Rejected**: "Startup" suggests a simple init sequence. "Bootloader" better communicates the validation and decision-making responsibilities, which is more educational for newcomers.

## Open Questions

1. **Rev1 Compatibility**: Should the bootloader work on Rev1 hardware (single button)?
   - Option A: Skip factory reset check on Rev1
   - Option B: Use config action (5-tap + hold) for Rev1 reset
   - Option C: Rev1 stays with current startup.c

2. **EEPROM Wear**: Should we track write cycles or implement wear leveling?
   - Probably unnecessary for settings that change rarely
   - ATtiny85 EEPROM rated for 100,000 cycles

3. **Boot Timeout**: Should there be a maximum boot time?
   - Current design: ~1.5s LED test + up to 3s reset check
   - Eurorack modules typically boot in <2s

4. **Settings Version Migration**: If EEPROM format changes in future firmware:
   - Bump magic number version
   - Old format treated as invalid → defaults

## Implementation Checklist

- [ ] Create `include/bootloader.h` with interface
- [ ] Create `src/bootloader.c` with implementation
- [ ] Add EEPROM simulation to mock HAL
- [ ] Create `test/unit/bootloader/test_bootloader.h`
- [ ] Update `unit_tests.c` to include bootloader tests
- [ ] Update `main.c` to use bootloader
- [ ] Delete `startup.c` and `startup.h`
- [ ] Update `src/CMakeLists.txt`
- [ ] Update `docs/ARCHITECTURE.md`
- [ ] Test on hardware (Rev1 or Rev2 prototype)
- [ ] Update README if boot behavior mentioned
