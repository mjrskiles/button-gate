#include "mock_hal.h"
#include <string.h>

// Per-pin state tracking (supports pins 0-7)
#define MOCK_NUM_PINS 8
static uint8_t mock_pin_states[MOCK_NUM_PINS] = {0};
static uint32_t vmock_millis = 0;

// Mock EEPROM (512 bytes, matching ATtiny85)
#define MOCK_EEPROM_SIZE 512
static uint8_t mock_eeprom[MOCK_EEPROM_SIZE];

// Mock ADC values (4 channels on ATtiny85)
#define MOCK_ADC_CHANNELS 4
static uint8_t mock_adc_values[MOCK_ADC_CHANNELS] = {0};

// The mock interface instance
// Note: Pin assignments use unique values for testability, even though
// Rev2 hardware shares pins (Neopixels on PB0, output LED from buffer).
static HalInterface mock_hal = {
    .max_pin            = MOCK_NUM_PINS - 1,  // 0-7 valid
    .button_a_pin       = 2,  // PB2 in Rev2
    .button_b_pin       = 4,  // PB4 in Rev2
    .sig_out_pin        = 1,  // PB1 in Rev2
    .led_mode_top_pin   = 5,  // Virtual pin for testing (Neopixel A in Rev2)
    .led_output_indicator_pin = 6,  // Virtual pin for testing (buffer LED in Rev2)
    .led_mode_bottom_pin = 7,  // Virtual pin for testing (Neopixel B in Rev2)
    .init               = mock_hal_init,
    .set_pin            = mock_set_pin,
    .clear_pin          = mock_clear_pin,
    .toggle_pin         = mock_toggle_pin,
    .read_pin           = mock_read_pin,
    .init_timer         = mock_init_timer0,
    .millis             = mock_millis,
    .delay_ms           = mock_delay_ms,
    .advance_time       = advance_mock_time,
    .reset_time         = reset_mock_time,
    .eeprom_read_byte   = mock_eeprom_read_byte,
    .eeprom_write_byte  = mock_eeprom_write_byte,
    .eeprom_read_word   = mock_eeprom_read_word,
    .eeprom_write_word  = mock_eeprom_write_word,
    .adc_read           = mock_adc_read,
    .wdt_enable         = mock_wdt_enable,
    .wdt_reset          = mock_wdt_reset,
    .wdt_disable        = mock_wdt_disable,
};

HalInterface *p_hal = &mock_hal;

void use_mock_hal(void) {
    p_hal = &mock_hal;
}

void mock_hal_init(void) {
    for (int i = 0; i < MOCK_NUM_PINS; i++) {
        mock_pin_states[i] = 0;
    }
    vmock_millis = 0;
    // Initialize EEPROM to 0xFF (erased state)
    memset(mock_eeprom, 0xFF, MOCK_EEPROM_SIZE);
    // Clear ADC values
    memset(mock_adc_values, 0, MOCK_ADC_CHANNELS);
}

void mock_set_pin(uint8_t pin) {
    if (pin < MOCK_NUM_PINS) {
        mock_pin_states[pin] = 1;
    }
}

void mock_clear_pin(uint8_t pin) {
    if (pin < MOCK_NUM_PINS) {
        mock_pin_states[pin] = 0;
    }
}

void mock_toggle_pin(uint8_t pin) {
    if (pin < MOCK_NUM_PINS) {
        mock_pin_states[pin] = !mock_pin_states[pin];
    }
}

uint8_t mock_read_pin(uint8_t pin) {
    if (pin < MOCK_NUM_PINS) {
        return mock_pin_states[pin];
    }
    return 0;
}

void mock_init_timer0(void) {
    // Do nothing
}

uint32_t mock_millis(void) {
    // Return the mocked time
    return vmock_millis;
}

void mock_delay_ms(uint32_t ms) {
    // In mock, delay just advances time - no actual blocking
    vmock_millis += ms;
}

void advance_mock_time(uint32_t ms) {
    vmock_millis += ms;
} 

void reset_mock_time(void) {
    vmock_millis = 0;
}

uint8_t mock_eeprom_read_byte(uint16_t addr) {
    if (addr < MOCK_EEPROM_SIZE) {
        return mock_eeprom[addr];
    }
    return 0xFF;
}

void mock_eeprom_write_byte(uint16_t addr, uint8_t value) {
    if (addr < MOCK_EEPROM_SIZE) {
        mock_eeprom[addr] = value;
    }
}

uint16_t mock_eeprom_read_word(uint16_t addr) {
    if (addr + 1 < MOCK_EEPROM_SIZE) {
        // Little-endian (AVR native byte order)
        return mock_eeprom[addr] | ((uint16_t)mock_eeprom[addr + 1] << 8);
    }
    return 0xFFFF;
}

void mock_eeprom_write_word(uint16_t addr, uint16_t value) {
    if (addr + 1 < MOCK_EEPROM_SIZE) {
        // Little-endian (AVR native byte order)
        mock_eeprom[addr] = value & 0xFF;
        mock_eeprom[addr + 1] = (value >> 8) & 0xFF;
    }
}

void mock_eeprom_clear(void) {
    memset(mock_eeprom, 0xFF, MOCK_EEPROM_SIZE);
}

uint16_t mock_eeprom_size(void) {
    return MOCK_EEPROM_SIZE;
}

uint8_t mock_adc_read(uint8_t channel) {
    if (channel < MOCK_ADC_CHANNELS) {
        return mock_adc_values[channel];
    }
    return 0;
}

void mock_adc_set_value(uint8_t channel, uint8_t value) {
    if (channel < MOCK_ADC_CHANNELS) {
        mock_adc_values[channel] = value;
    }
}

// Watchdog stubs (no-op in tests)
void mock_wdt_enable(void) {}
void mock_wdt_reset(void) {}
void mock_wdt_disable(void) {}