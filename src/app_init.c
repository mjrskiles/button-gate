#include "app_init.h"
#include "utility/delay.h"

// Size of AppSettings struct for iteration
#define APP_SETTINGS_SIZE sizeof(AppSettings)

/**
 * Calculate XOR checksum over settings struct.
 */
static uint8_t calculate_checksum(const AppSettings *settings) {
    const uint8_t *data = (const uint8_t *)settings;
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < APP_SETTINGS_SIZE; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/**
 * Provide visual feedback that factory reset is pending.
 * Toggles LEDs while waiting for buttons to be held.
 */
static void reset_feedback_tick(void) {
    p_hal->toggle_pin(p_hal->led_mode_top_pin);
    p_hal->toggle_pin(p_hal->led_mode_bottom_pin);
}

/**
 * Provide visual feedback that factory reset completed.
 */
static void reset_complete_feedback(void) {
    // All LEDs solid for confirmation
    p_hal->set_pin(p_hal->led_mode_top_pin);
    p_hal->set_pin(p_hal->led_mode_bottom_pin);
    p_hal->set_pin(p_hal->led_output_indicator_pin);
    util_delay_ms(500);
    p_hal->clear_pin(p_hal->led_mode_top_pin);
    p_hal->clear_pin(p_hal->led_mode_bottom_pin);
    p_hal->clear_pin(p_hal->led_output_indicator_pin);
}

/**
 * Provide visual feedback that defaults are being used.
 * Double-blink pattern on output LED.
 */
static void defaults_feedback(void) {
    for (uint8_t i = 0; i < 2; i++) {
        p_hal->set_pin(p_hal->led_output_indicator_pin);
        util_delay_ms(100);
        p_hal->clear_pin(p_hal->led_output_indicator_pin);
        util_delay_ms(100);
    }
    util_delay_ms(200);
    for (uint8_t i = 0; i < 2; i++) {
        p_hal->set_pin(p_hal->led_output_indicator_pin);
        util_delay_ms(100);
        p_hal->clear_pin(p_hal->led_output_indicator_pin);
        util_delay_ms(100);
    }
}

void app_init_get_defaults(AppSettings *settings) {
    if (!settings) return;

    settings->mode = MODE_GATE;
    settings->cv_function = 0;  // Default CV function
    settings->param1 = 0;
    settings->param2 = 0;
    // Clear reserved bytes
    for (uint8_t i = 0; i < 4; i++) {
        settings->reserved[i] = 0;
    }
}

bool app_init_check_factory_reset(void) {
    // Check if both buttons are currently pressed
    if (!p_hal->read_pin(p_hal->button_a_pin) ||
        !p_hal->read_pin(p_hal->button_b_pin)) {
        return false;
    }

    // Both pressed - start counting with visual feedback
    uint32_t start_time = p_hal->millis();
    uint32_t last_blink = start_time;

    while ((p_hal->millis() - start_time) < APP_INIT_RESET_HOLD_MS) {
        // Blink LEDs for feedback
        if ((p_hal->millis() - last_blink) >= APP_INIT_RESET_BLINK_MS) {
            reset_feedback_tick();
            last_blink = p_hal->millis();
        }

        // If either button released, abort
        if (!p_hal->read_pin(p_hal->button_a_pin) ||
            !p_hal->read_pin(p_hal->button_b_pin)) {
            // Turn off LEDs and return
            p_hal->clear_pin(p_hal->led_mode_top_pin);
            p_hal->clear_pin(p_hal->led_mode_bottom_pin);
            return false;
        }

        // Small delay to avoid busy-waiting too hard
        util_delay_ms(APP_INIT_RESET_POLL_MS);
    }

    // Held long enough - show confirmation
    reset_complete_feedback();
    return true;
}

void app_init_clear_eeprom(void) {
    // Clear magic number to invalidate
    p_hal->eeprom_write_word(EEPROM_MAGIC_ADDR, 0xFFFF);
}

void app_init_save_settings(const AppSettings *settings) {
    if (!settings) return;

    // Write magic number
    p_hal->eeprom_write_word(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);

    // Write schema version
    p_hal->eeprom_write_byte(EEPROM_SCHEMA_ADDR, SETTINGS_SCHEMA_VERSION);

    // Write settings struct byte by byte
    const uint8_t *data = (const uint8_t *)settings;
    for (uint8_t i = 0; i < APP_SETTINGS_SIZE; i++) {
        p_hal->eeprom_write_byte(EEPROM_SETTINGS_ADDR + i, data[i]);
    }

    // Write checksum
    p_hal->eeprom_write_byte(EEPROM_CHECKSUM_ADDR, calculate_checksum(settings));
}

/**
 * Validate and load settings from EEPROM.
 *
 * @param settings Pointer to settings struct to populate
 * @return true if settings are valid and loaded, false otherwise
 */
static bool load_settings(AppSettings *settings) {
    // Level 1: Check magic number
    uint16_t magic = p_hal->eeprom_read_word(EEPROM_MAGIC_ADDR);
    if (magic != EEPROM_MAGIC_VALUE) {
        return false;
    }

    // Level 2: Check schema version
    uint8_t schema = p_hal->eeprom_read_byte(EEPROM_SCHEMA_ADDR);
    if (schema != SETTINGS_SCHEMA_VERSION) {
        // Future: could attempt migration here
        // For now, treat as invalid
        return false;
    }

    // Level 3: Read settings and verify checksum
    uint8_t *data = (uint8_t *)settings;
    for (uint8_t i = 0; i < APP_SETTINGS_SIZE; i++) {
        data[i] = p_hal->eeprom_read_byte(EEPROM_SETTINGS_ADDR + i);
    }

    uint8_t stored_checksum = p_hal->eeprom_read_byte(EEPROM_CHECKSUM_ADDR);
    uint8_t calculated_checksum = calculate_checksum(settings);
    if (stored_checksum != calculated_checksum) {
        return false;
    }

    // Level 4: Range validation
    if (settings->mode > MODE_TOGGLE) {
        return false;
    }

    return true;
}

AppInitResult app_init_run(AppSettings *settings) {
    if (!settings) return APP_INIT_OK_DEFAULTS;

    // Check for factory reset (both buttons held)
    if (app_init_check_factory_reset()) {
        app_init_clear_eeprom();
        app_init_get_defaults(settings);
        app_init_save_settings(settings);
        return APP_INIT_OK_FACTORY_RESET;
    }

    // Attempt to load settings from EEPROM
    if (load_settings(settings)) {
        return APP_INIT_OK;
    }

    // Validation failed - use defaults
    app_init_get_defaults(settings);
    defaults_feedback();
    return APP_INIT_OK_DEFAULTS;
}
