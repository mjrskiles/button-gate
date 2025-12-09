#include "output/neopixel.h"

#if defined(__AVR__)

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/**
 * @file neopixel.c
 * @brief WS2812B driver for ATtiny85 at 1 MHz
 *
 * Bit-banged driver with cycle-accurate timing for WS2812B LEDs.
 * At 1 MHz, each cycle is 1µs, which is challenging for WS2812B timing.
 *
 * WS2812B timing requirements:
 * - T0H: 350ns ±150ns (0.2-0.5µs)
 * - T0L: 800ns ±150ns (0.65-0.95µs)
 * - T1H: 700ns ±150ns (0.55-0.85µs)
 * - T1L: 600ns ±150ns (0.45-0.75µs)
 * - Reset: >50µs
 *
 * At 1 MHz (1µs per cycle), we have very limited granularity.
 * We use the shortest possible pulses and rely on the generous tolerances.
 *
 * Data pin: PB0
 */

// Neopixel data pin
#define NEOPIXEL_PIN    PB0
#define NEOPIXEL_PORT   PORTB
#define NEOPIXEL_DDR    DDRB

// LED buffer in GRB order (WS2812B native format)
static uint8_t led_buffer[NEOPIXEL_COUNT * 3];
static bool buffer_dirty = false;

void neopixel_init(void) {
    // Configure pin as output, initially low
    NEOPIXEL_DDR |= (1 << NEOPIXEL_PIN);
    NEOPIXEL_PORT &= ~(1 << NEOPIXEL_PIN);

    neopixel_clear();
    neopixel_flush();
}

void neopixel_set_color(uint8_t index, NeopixelColor color) {
    if (index >= NEOPIXEL_COUNT) return;

    uint8_t offset = index * 3;
    // WS2812B expects GRB order
    led_buffer[offset + 0] = color.g;
    led_buffer[offset + 1] = color.r;
    led_buffer[offset + 2] = color.b;
    buffer_dirty = true;
}

void neopixel_set_rgb(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= NEOPIXEL_COUNT) return;

    uint8_t offset = index * 3;
    led_buffer[offset + 0] = g;  // GRB order
    led_buffer[offset + 1] = r;
    led_buffer[offset + 2] = b;
    buffer_dirty = true;
}

NeopixelColor neopixel_get_color(uint8_t index) {
    NeopixelColor color = {0, 0, 0};
    if (index >= NEOPIXEL_COUNT) return color;

    uint8_t offset = index * 3;
    color.g = led_buffer[offset + 0];
    color.r = led_buffer[offset + 1];
    color.b = led_buffer[offset + 2];
    return color;
}

void neopixel_clear(void) {
    for (uint8_t i = 0; i < NEOPIXEL_COUNT * 3; i++) {
        led_buffer[i] = 0;
    }
    buffer_dirty = true;
}

bool neopixel_is_dirty(void) {
    return buffer_dirty;
}

/**
 * Send a single byte to the LED chain.
 *
 * At 1 MHz, we have ~1µs per instruction cycle.
 * WS2812B is forgiving with timing, so we use:
 * - Bit 1: HIGH for ~2 cycles, LOW for ~1 cycle
 * - Bit 0: HIGH for ~1 cycle, LOW for ~2 cycles
 *
 * The inline assembly ensures consistent timing.
 */
static void send_byte(uint8_t byte) {
    uint8_t bit_count = 8;

    // Unrolled loop for consistent timing
    // Each bit takes approximately 3 cycles
    asm volatile(
        "send_bit_%=:"                      "\n\t"
        "sbrc %[byte], 7"                   "\n\t"  // Skip if bit 7 clear
        "rjmp send_one_%="                  "\n\t"

        // Send 0: short high, long low
        "sbi %[port], %[pin]"               "\n\t"  // 1 cycle high
        "cbi %[port], %[pin]"               "\n\t"  // then low
        "nop"                               "\n\t"  // extend low time
        "rjmp next_bit_%="                  "\n\t"

        "send_one_%=:"                      "\n\t"
        // Send 1: long high, short low
        "sbi %[port], %[pin]"               "\n\t"  // high
        "nop"                               "\n\t"  // extend high time
        "cbi %[port], %[pin]"               "\n\t"  // then low

        "next_bit_%=:"                      "\n\t"
        "lsl %[byte]"                       "\n\t"  // shift to next bit
        "dec %[count]"                      "\n\t"
        "brne send_bit_%="                  "\n\t"

        : [byte] "+r" (byte),
          [count] "+r" (bit_count)
        : [port] "I" (_SFR_IO_ADDR(NEOPIXEL_PORT)),
          [pin] "I" (NEOPIXEL_PIN)
    );
}

void neopixel_flush(void) {
    if (!buffer_dirty) return;

    // Disable interrupts for timing-critical transmission
    uint8_t sreg = SREG;
    cli();

    // Send all bytes
    for (uint8_t i = 0; i < NEOPIXEL_COUNT * 3; i++) {
        send_byte(led_buffer[i]);
    }

    // Restore interrupt state
    SREG = sreg;

    // Reset pulse (>50µs low)
    _delay_us(60);

    buffer_dirty = false;
}

#endif /* __AVR__ */
