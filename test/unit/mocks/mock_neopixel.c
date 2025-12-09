#include "output/neopixel.h"
#include "mocks/mock_neopixel.h"
#include <string.h>

/**
 * @file mock_neopixel.c
 * @brief Mock neopixel driver for tests and simulator
 *
 * This implementation is used when not building for AVR.
 * It stores LED values in a buffer for test inspection.
 */

#if !defined(__AVR__)

// LED buffer (RGB format for easier inspection, unlike GRB on real hardware)
static NeopixelColor mock_leds[NEOPIXEL_COUNT];
static bool buffer_dirty = false;
static int flush_count = 0;

void neopixel_init(void) {
    mock_neopixel_reset();
}

void neopixel_set_color(uint8_t index, NeopixelColor color) {
    if (index >= NEOPIXEL_COUNT) return;
    mock_leds[index] = color;
    buffer_dirty = true;
}

void neopixel_set_rgb(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= NEOPIXEL_COUNT) return;
    mock_leds[index].r = r;
    mock_leds[index].g = g;
    mock_leds[index].b = b;
    buffer_dirty = true;
}

NeopixelColor neopixel_get_color(uint8_t index) {
    if (index >= NEOPIXEL_COUNT) {
        NeopixelColor black = {0, 0, 0};
        return black;
    }
    return mock_leds[index];
}

void neopixel_clear(void) {
    memset(mock_leds, 0, sizeof(mock_leds));
    buffer_dirty = true;
}

bool neopixel_is_dirty(void) {
    return buffer_dirty;
}

void neopixel_flush(void) {
    if (!buffer_dirty) return;
    flush_count++;
    buffer_dirty = false;
}

// Mock-specific functions

void mock_neopixel_reset(void) {
    memset(mock_leds, 0, sizeof(mock_leds));
    buffer_dirty = false;
    flush_count = 0;
}

int mock_neopixel_get_flush_count(void) {
    return flush_count;
}

bool mock_neopixel_check_color(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= NEOPIXEL_COUNT) return false;
    return mock_leds[index].r == r &&
           mock_leds[index].g == g &&
           mock_leds[index].b == b;
}

#endif /* !__AVR__ */
