#ifndef GK_MOCK_NEOPIXEL_H
#define GK_MOCK_NEOPIXEL_H

#include "output/neopixel.h"

/**
 * @file mock_neopixel.h
 * @brief Mock neopixel driver for tests and simulator
 *
 * Provides test inspection functions to verify LED state.
 */

/**
 * Reset mock state (clear LEDs, reset counters).
 */
void mock_neopixel_reset(void);

/**
 * Get number of times flush has been called.
 */
int mock_neopixel_get_flush_count(void);

/**
 * Check if a specific LED matches expected color.
 *
 * @param index LED index
 * @param r     Expected red
 * @param g     Expected green
 * @param b     Expected blue
 * @return      true if color matches
 */
bool mock_neopixel_check_color(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

#endif /* GK_MOCK_NEOPIXEL_H */
