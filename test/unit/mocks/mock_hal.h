#ifndef GK_TEST_MOCKS_MOCK_HAL_H
#define GK_TEST_MOCKS_MOCK_HAL_H

#include "hardware/hal_interface.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initializes the mock HAL state
 * 
 * Resets pin states and internal timers to default values.
 */
void mock_hal_init(void);

/**
 * @brief Sets a mock pin to high state
 * @param pin The pin number to set
 */
void mock_set_pin(uint8_t pin);

/**
 * @brief Sets a mock pin to low state
 * @param pin The pin number to clear
 */
void mock_clear_pin(uint8_t pin);

/**
 * @brief Toggles a mock pin's state
 * @param pin The pin number to toggle
 */
void mock_toggle_pin(uint8_t pin);

/**
 * @brief Reads the current state of a mock pin
 * @param pin The pin number to read
 * @return Current state of the pin (0 or 1)
 */
uint8_t mock_read_pin(uint8_t pin);

/**
 * @brief Mock initialization of Timer0
 * 
 * Does nothing in the mock implementation.
 */
void mock_init_timer0(void);

/**
 * @brief Returns the current mock system time in milliseconds
 * @return Current mock time in milliseconds
 */
uint32_t mock_millis(void);

/**
 * @brief Mock delay - advances mock time instead of blocking
 * @param ms Number of milliseconds to "delay" (advances mock time)
 */
void mock_delay_ms(uint32_t ms);

/**
 * @brief Switches the global HAL interface to use this mock implementation
 */
void use_mock_hal(void);

/**
 * @brief Advances the mock system time by the specified number of milliseconds
 * @param ms Number of milliseconds to advance
 */
void advance_mock_time(uint32_t ms);

/**
 * @brief Resets the mock system time to 0
 */
void reset_mock_time(void);

/**
 * @brief Mock EEPROM read byte
 * @param addr EEPROM address to read from
 * @return The byte value at the specified address
 */
uint8_t mock_eeprom_read_byte(uint16_t addr);

/**
 * @brief Mock EEPROM write byte
 * @param addr EEPROM address to write to
 * @param value The byte value to write
 */
void mock_eeprom_write_byte(uint16_t addr, uint8_t value);

/**
 * @brief Mock EEPROM read word (16-bit)
 * @param addr EEPROM address to read from
 * @return The word value at the specified address
 */
uint16_t mock_eeprom_read_word(uint16_t addr);

/**
 * @brief Mock EEPROM write word (16-bit)
 * @param addr EEPROM address to write to
 * @param value The word value to write
 */
void mock_eeprom_write_word(uint16_t addr, uint16_t value);

/**
 * @brief Clears all mock EEPROM contents (sets all bytes to 0xFF)
 */
void mock_eeprom_clear(void);

/**
 * @brief Gets the size of the mock EEPROM
 * @return Size of mock EEPROM in bytes
 */
uint16_t mock_eeprom_size(void);

/**
 * @brief Mock ADC read (per ADR-004)
 * @param channel ADC channel to read (0-3)
 * @return 8-bit ADC value (0-255)
 */
uint8_t mock_adc_read(uint8_t channel);

/**
 * @brief Set the mock ADC value for a channel
 * @param channel ADC channel (0-3)
 * @param value 8-bit ADC value to return on read
 */
void mock_adc_set_value(uint8_t channel, uint8_t value);

/**
 * @brief Mock watchdog enable (no-op in tests)
 */
void mock_wdt_enable(void);

/**
 * @brief Mock watchdog reset (no-op in tests)
 */
void mock_wdt_reset(void);

/**
 * @brief Mock watchdog disable (no-op in tests)
 */
void mock_wdt_disable(void);

#endif /* GK_TEST_MOCKS_MOCK_HAL_H */
