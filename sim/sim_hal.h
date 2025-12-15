#ifndef GK_SIM_HAL_H
#define GK_SIM_HAL_H

#include "hardware/hal_interface.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @file sim_hal.h
 * @brief x86 simulator HAL interface
 *
 * Pure hardware abstraction layer for x86 simulator.
 * Display is handled separately by renderers.
 */

// Number of simulated LEDs
#define SIM_NUM_LEDS 2

/**
 * Get the simulator HAL interface.
 * Assign to p_hal before running app code.
 */
HalInterface* sim_get_hal(void);

/**
 * Input state setters (for input sources).
 * These set the simulated hardware input state.
 */
void sim_set_button_a(bool pressed);
void sim_set_button_b(bool pressed);

/**
 * Set CV input voltage level (0-255, maps to 0-5V).
 * The digital state is derived via hysteresis in cv_input module.
 */
void sim_set_cv_voltage(uint8_t adc_value);

/**
 * Adjust CV voltage by delta (-255 to +255).
 * Clamps to valid range.
 */
void sim_adjust_cv_voltage(int16_t delta);

/**
 * Input state getters.
 */
bool sim_get_button_a(void);
bool sim_get_button_b(void);

/**
 * Get current CV voltage level (0-255 ADC value).
 */
uint8_t sim_get_cv_voltage(void);

/**
 * Output state getter.
 */
bool sim_get_output(void);

/**
 * LED state control (for neopixel simulation).
 */
void sim_set_led(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void sim_get_led(uint8_t index, uint8_t *r, uint8_t *g, uint8_t *b);

/**
 * Get current simulation time.
 */
uint32_t sim_get_time(void);

/**
 * Reset simulation time to 0.
 */
void sim_reset_time(void);

#endif /* GK_SIM_HAL_H */
