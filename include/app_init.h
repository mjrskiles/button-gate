#ifndef GK_APP_INIT_H
#define GK_APP_INIT_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/hal_interface.h"
#include "state/mode.h"

/**
 * @file app_init.h
 * @brief Application initialization for Gatekeeper
 *
 * Handles startup tasks before entering the main application loop:
 * - Factory reset detection (both buttons held on startup)
 * - EEPROM settings validation and loading
 * - Graceful degradation to defaults on EEPROM errors
 *
 * Initialization sequence:
 * 1. Check for factory reset (both buttons held for APP_INIT_RESET_HOLD_MS)
 *    - If triggered: clear EEPROM, write defaults, continue
 * 2. Load settings from EEPROM
 * 3. Validate settings (magic number, schema version, checksum, ranges)
 *    - If invalid: use defaults, signal warning
 * 4. Return to main for RUN mode entry
 */

// Timing constants
#define APP_INIT_RESET_HOLD_MS       3000  // Hold time for factory reset
#define APP_INIT_RESET_POLL_MS         50  // Polling interval during reset check
#define APP_INIT_RESET_BLINK_MS       100  // LED blink rate during reset pending

// EEPROM layout
#define EEPROM_MAGIC_ADDR           0x00    // 2 bytes: magic number
#define EEPROM_SCHEMA_ADDR          0x02    // 1 byte: schema version
#define EEPROM_SETTINGS_ADDR        0x03    // Settings struct starts here
#define EEPROM_CHECKSUM_ADDR        0x10    // 1 byte: XOR checksum of settings

// Magic number: "GK" in ASCII (0x474B)
#define EEPROM_MAGIC_VALUE          0x474B

// Schema version - increment when settings struct changes
#define SETTINGS_SCHEMA_VERSION     1

/**
 * Initialization result codes
 */
typedef enum {
    APP_INIT_OK,                    // Normal init, settings loaded from EEPROM
    APP_INIT_OK_DEFAULTS,           // Initialized with defaults (EEPROM invalid/empty)
    APP_INIT_OK_FACTORY_RESET,      // Factory reset performed, using defaults
} AppInitResult;

/**
 * Application settings loaded during initialization
 *
 * This struct is stored in EEPROM. When changing this struct:
 * 1. Increment SETTINGS_SCHEMA_VERSION
 * 2. Update app_init_migrate_settings() if migration is possible
 * 3. Update EEPROM_CHECKSUM_ADDR if struct size changes
 */
typedef struct __attribute__((packed)) {
    uint8_t mode;               // CVMode enum value
    uint8_t cv_function;        // CV input function (for Rev2)
    uint8_t param1;             // Mode-specific parameter 1
    uint8_t param2;             // Mode-specific parameter 2
    uint8_t reserved[4];        // Future expansion (total: 8 bytes)
} AppSettings;

/**
 * Execute the initialization sequence.
 *
 * Checks for factory reset, loads/validates EEPROM settings, and populates
 * the settings struct. On any validation failure, defaults are used and
 * the appropriate result code is returned.
 *
 * @param settings  Pointer to struct that will be populated with loaded settings
 * @return          Result code indicating how initialization completed
 */
AppInitResult app_init_run(AppSettings *settings);

/**
 * Get default settings.
 *
 * Populates the settings struct with safe default values.
 * Used for factory reset and when EEPROM is invalid.
 *
 * @param settings  Pointer to struct to populate with defaults
 */
void app_init_get_defaults(AppSettings *settings);

/**
 * Save settings to EEPROM.
 *
 * Writes the settings struct to EEPROM with magic number, schema version,
 * and checksum. Uses eeprom_update_* functions to minimize write cycles.
 *
 * @param settings  Pointer to settings to save
 */
void app_init_save_settings(const AppSettings *settings);

/**
 * Check if factory reset is being requested.
 *
 * Monitors both buttons and returns true if held for APP_INIT_RESET_HOLD_MS.
 * Provides visual feedback (blinking LEDs) while waiting.
 *
 * @return true if factory reset was requested, false otherwise
 */
bool app_init_check_factory_reset(void);

/**
 * Clear all EEPROM settings.
 *
 * Erases the magic number and settings area, forcing next init to use defaults.
 */
void app_init_clear_eeprom(void);

#endif /* GK_APP_INIT_H */
