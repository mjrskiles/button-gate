#include "hardware/hal.h"

static HalInterface default_hal = {
    .init         = hal_init,
    .set_pin      = hal_set_pin,
    .clear_pin    = hal_clear_pin,
    .toggle_pin   = hal_toggle_pin,
    .read_pin     = hal_read_pin,
    .init_timer0  = hal_init_timer0,
    .millis       = hal_millis,
};

HalInterface *p_hal = &default_hal;

/*
    Initializes I/O pins for this application.

    - Disables the ADC (if analog functions aren't needed).
    - Configures UART_TX and LED_GATE_PIN, LED_TOGGLE_PIN, LED_PULSE_PIN as outputs.
    - Sets initial states:
        * UART_TX: idle high.
        * LED_GATE_PIN: set off
        * LED_TOGGLE_PIN: set off
        * LED_PULSE_PIN: set off
*/
void hal_init(void) {

    // Disable ADC to ensure that all pins function as digital I/O
    ADCSRA &= ~(1 << ADEN);

    // Set button pin as input
    DDRB &= ~(1 << BUTTON_PIN);

    // Set signal out pin as output
    DDRB |= (1 << SIG_OUT_PIN);

    // LED pin configuration:
    // Set all LED pins as outputs
    DDRB |= (1 << LED_MODE_TOP_PIN) | (1 << LED_OUTPUT_INDICATOR_PIN) | (1 << LED_MODE_BOTTOM_PIN);
    
    // Configure LED off initially.
    PORTB &= ~((1 << LED_MODE_TOP_PIN) | (1 << LED_OUTPUT_INDICATOR_PIN) | (1 << LED_MODE_BOTTOM_PIN));

    // Configure signal out pin as low
    PORTB &= ~(1 << SIG_OUT_PIN);

    // Ensure button pin is not pulled up
    PORTB &= ~(1 << BUTTON_PIN);
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

/**
 * Initializes Timer0 for millisecond timing
 * Uses Timer0 in CTC mode with a 8 prescaler
 * For 1MHz clock: 1000000/8/125 = 1000Hz (1ms)
 */
void hal_init_timer0(void) {
    // Set Timer0 to CTC mode
    TCCR0A = (1 << WGM01);
    
    // Set prescaler to 8
    TCCR0B = (1 << CS01);
    
    // Set compare value for 1ms intervals (with 1MHz clock)
    OCR0A = 124;  // (1000000/8/1000) - 1
    
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