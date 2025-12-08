#include "hardware/hal.h"
#include <avr/eeprom.h>
#include <avr/sleep.h>

static HalInterface default_hal = {
    .max_pin            = HAL_MAX_PIN,
    .button_a_pin       = BUTTON_A_PIN,
    .button_b_pin       = BUTTON_B_PIN,
    .sig_out_pin        = SIG_OUT_PIN,
    .led_mode_top_pin   = LED_MODE_TOP_PIN,
    .led_output_indicator_pin = LED_OUTPUT_INDICATOR_PIN,
    .led_mode_bottom_pin = LED_MODE_BOTTOM_PIN,
    .init               = hal_init,
    .set_pin            = hal_set_pin,
    .clear_pin          = hal_clear_pin,
    .toggle_pin         = hal_toggle_pin,
    .read_pin           = hal_read_pin,
    .init_timer         = hal_init_timer0,
    .millis             = hal_millis,
    .delay_ms           = hal_delay_ms,
    .advance_time       = hal_advance_time,
    .reset_time         = hal_reset_time,
    .eeprom_read_byte   = hal_eeprom_read_byte,
    .eeprom_write_byte  = hal_eeprom_write_byte,
    .eeprom_read_word   = hal_eeprom_read_word,
    .eeprom_write_word  = hal_eeprom_write_word,
};

HalInterface *p_hal = &default_hal;

/**
 * Initializes I/O pins for Rev2 hardware.
 *
 * Rev2 Pin Assignments:
 * - PB0: Neopixel data (output, directly driven)
 * - PB1: CV output (output)
 * - PB2: Button A (input, no pull-up - external pull-down)
 * - PB3: CV input (input, future use)
 * - PB4: Button B (input, no pull-up - external pull-down)
 * - PB5: RESET (reserved)
 *
 * Also initializes Timer0 for millisecond timing.
 */
void hal_init(void) {
    // Disable ADC to ensure that all pins function as digital I/O
    ADCSRA &= ~(1 << ADEN);

    // Configure button pins as inputs
    DDRB &= ~((1 << BUTTON_A_PIN) | (1 << BUTTON_B_PIN));

    // Ensure button pins have no internal pull-up (external pull-down expected)
    PORTB &= ~((1 << BUTTON_A_PIN) | (1 << BUTTON_B_PIN));

    // Set signal out pin as output, initially low
    DDRB |= (1 << SIG_OUT_PIN);
    PORTB &= ~(1 << SIG_OUT_PIN);

    // Set Neopixel pin as output, initially low
    DDRB |= (1 << NEOPIXEL_PIN);
    PORTB &= ~(1 << NEOPIXEL_PIN);

    // CV input pin as input (future use)
    DDRB &= ~(1 << CV_IN_PIN);

    // Initialize Timer0
    hal_init_timer0();
}

/**
 * Sets the specified pin high.
 * 
 * @param pin The pin number to set high
 */
void hal_set_pin(uint8_t pin) {
    PORTB |= (1 << pin);
}

/**
 * Sets the specified pin low.
 * 
 * @param pin The pin number to set low
 */
void hal_clear_pin(uint8_t pin) {
    PORTB &= ~(1 << pin);
}

/**
 * Toggles the specified pin (inverts its current state).
 * 
 * @param pin The pin number to toggle
 */
void hal_toggle_pin(uint8_t pin) {
    PORTB ^= (1 << pin);
}

/**
 * Reads the current state of the specified pin.
 * 
 * @param pin The pin number to read
 * @return 1 if the pin is high, 0 if the pin is low
 */
uint8_t hal_read_pin(uint8_t pin) {
    return (PINB & (1 << pin)) != 0;
}

// Global variable to keep track of milliseconds
static volatile uint32_t timer0_millis = 0;

// Timer configuration - derived from F_CPU (set in CMakeLists.txt)
#ifndef F_CPU
#error "F_CPU must be defined (e.g., 1000000UL for 1MHz, 8000000UL for 8MHz)"
#endif

#define TIMER0_PRESCALER      8
#define TIMER0_TARGET_HZ      1000  // 1ms intervals
#define TIMER0_COMPARE_VALUE  ((F_CPU / TIMER0_PRESCALER / TIMER0_TARGET_HZ) - 1)

// Compile-time validation
#if TIMER0_COMPARE_VALUE > 255
#error "F_CPU too high for 8-bit timer with prescaler 8. Increase prescaler."
#endif
#if TIMER0_COMPARE_VALUE < 1
#error "F_CPU too low for accurate 1ms timing with prescaler 8."
#endif

/**
 * Initializes Timer0 for millisecond timing.
 * Uses CTC mode with prescaler 8.
 * Compare value is calculated from F_CPU at compile time.
 */
void hal_init_timer0(void) {
    // Set Timer0 to CTC mode
    TCCR0A = (1 << WGM01);

    // Set prescaler to 8 (CS01)
    TCCR0B = (1 << CS01);

    // Set compare value for 1ms intervals
    OCR0A = TIMER0_COMPARE_VALUE;

    // Enable Timer0 compare match interrupt
    TIMSK |= (1 << OCIE0A);

    // Enable global interrupts
    sei();
}

/**
 * Timer0 compare match interrupt handler
 * Increments millisecond counter
 */
ISR(TIMER0_COMPA_vect) {
    timer0_millis++;
}

/**
 * Returns the number of milliseconds since the program started
 * 
 * @return Number of milliseconds since program start
 */
uint32_t hal_millis(void) {
    uint32_t m;
    
    // Disable interrupts while reading timer0_millis
    cli();
    m = timer0_millis;
    sei();
    
    return m;
}

/**
 * Blocking delay for the specified number of milliseconds.
 *
 * Uses Idle sleep mode for power efficiency. The CPU sleeps between
 * Timer0 interrupts (1ms intervals), waking briefly to check if the
 * delay has elapsed. This reduces power consumption by ~90% compared
 * to busy-waiting.
 *
 * @param ms Number of milliseconds to delay
 */
void hal_delay_ms(uint32_t ms) {
    uint32_t start_time = hal_millis();

    set_sleep_mode(SLEEP_MODE_IDLE);

    while (hal_millis() - start_time < ms) {
        sleep_mode();  // Sleep until Timer0 interrupt (1ms)
    }
}

void hal_advance_time(uint32_t ms) {
    // Disable interrupts while updating timer0_millis
    cli();
    timer0_millis += ms;
    sei();
}

void hal_reset_time(void) {
    // Disable interrupts while updating timer0_millis
    cli();
    timer0_millis = 0;
    sei();
}

/**
 * Reads a byte from EEPROM.
 *
 * @param addr EEPROM address to read from
 * @return The byte value at the specified address
 */
uint8_t hal_eeprom_read_byte(uint16_t addr) {
    return eeprom_read_byte((const uint8_t *)addr);
}

/**
 * Writes a byte to EEPROM.
 * Uses eeprom_update_byte to minimize write cycles (only writes if value differs).
 *
 * @param addr EEPROM address to write to
 * @param value The byte value to write
 */
void hal_eeprom_write_byte(uint16_t addr, uint8_t value) {
    eeprom_update_byte((uint8_t *)addr, value);
}

/**
 * Reads a 16-bit word from EEPROM.
 *
 * @param addr EEPROM address to read from
 * @return The word value at the specified address
 */
uint16_t hal_eeprom_read_word(uint16_t addr) {
    return eeprom_read_word((const uint16_t *)addr);
}

/**
 * Writes a 16-bit word to EEPROM.
 * Uses eeprom_update_word to minimize write cycles (only writes if value differs).
 *
 * @param addr EEPROM address to write to
 * @param value The word value to write
 */
void hal_eeprom_write_word(uint16_t addr, uint16_t value) {
    eeprom_update_word((uint16_t *)addr, value);
}