#ifndef MOCK_HAL_H
#define MOCK_HAL_H

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

#endif // MOCK_HAL_H
