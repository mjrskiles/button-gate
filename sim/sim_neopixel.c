#include "output/neopixel.h"
#include "sim_hal.h"
#include <string.h>

/**
 * @file sim_neopixel.c
 * @brief Simulator neopixel driver
 *
 * Routes neopixel colors to the simulator display via sim_set_led().
 * Used instead of mock_neopixel.c when building the simulator.
 */

// LED buffer (RGB format)
static NeopixelColor sim_leds[NEOPIXEL_COUNT];
static bool buffer_dirty = false;

void neopixel_init(void) {
    memset(sim_leds, 0, sizeof(sim_leds));
    buffer_dirty = false;

    // Clear display LEDs
    for (int i = 0; i < NEOPIXEL_COUNT; i++) {
        sim_set_led(i, 0, 0, 0);
    }
}

void neopixel_set_color(uint8_t index, NeopixelColor color) {
    if (index >= NEOPIXEL_COUNT) return;
    sim_leds[index] = color;
    buffer_dirty = true;
}

void neopixel_set_rgb(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= NEOPIXEL_COUNT) return;
    sim_leds[index].r = r;
    sim_leds[index].g = g;
    sim_leds[index].b = b;
    buffer_dirty = true;
}

NeopixelColor neopixel_get_color(uint8_t index) {
    if (index >= NEOPIXEL_COUNT) {
        NeopixelColor black = {0, 0, 0};
        return black;
    }
    return sim_leds[index];
}

void neopixel_clear(void) {
    memset(sim_leds, 0, sizeof(sim_leds));
    buffer_dirty = true;
}

bool neopixel_is_dirty(void) {
    return buffer_dirty;
}

void neopixel_flush(void) {
    if (!buffer_dirty) return;

    // Update simulator display
    for (int i = 0; i < NEOPIXEL_COUNT; i++) {
        sim_set_led(i, sim_leds[i].r, sim_leds[i].g, sim_leds[i].b);
    }

    buffer_dirty = false;
}
