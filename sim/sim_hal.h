#ifndef GK_SIM_HAL_H
#define GK_SIM_HAL_H

#include "hardware/hal_interface.h"
#include <stdbool.h>

/**
 * @file sim_hal.h
 * @brief x86 simulator HAL interface
 *
 * Pure hardware abstraction layer for x86 simulator.
 * Display is handled separately by renderers.
 */

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
void sim_set_cv_in(bool high);

/**
 * Input state getters.
 */
bool sim_get_button_a(void);
bool sim_get_button_b(void);

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

#endif /* GK_SIM_HAL_H */
