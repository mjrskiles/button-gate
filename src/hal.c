#include "hal.h"

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
void init_pins(void) {
    // Disable ADC to ensure that all pins function as digital I/O
    ADCSRA &= ~(1 << ADEN);

    // Set button pin as input
    DDRB &= ~(1 << BUTTON_PIN);

    // Set signal out pin as output
    DDRB |= (1 << SIG_OUT_PIN);

    // LED pin configuration:
    // Set all LED pins as outputs
    DDRB |= (1 << LED_GATE_PIN) | (1 << LED_TOGGLE_PIN) | (1 << LED_PULSE_PIN);
    
    // Configure LED off initially.
    PORTB &= ~((1 << LED_GATE_PIN) | (1 << LED_TOGGLE_PIN) | (1 << LED_PULSE_PIN));

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
void set_pin(uint8_t pin) {
    PORTB |= (1 << pin);
}

/**
 * Sets the specified pin low.
 * 
 * @param pin The pin number to set low
 */
void clear_pin(uint8_t pin) {
    PORTB &= ~(1 << pin);
}

/**
 * Toggles the specified pin (inverts its current state).
 * 
 * @param pin The pin number to toggle
 */
void toggle_pin(uint8_t pin) {
    PORTB ^= (1 << pin);
}

/**
 * Reads the current state of the specified pin.
 * 
 * @param pin The pin number to read
 * @return 1 if the pin is high, 0 if the pin is low
 */
uint8_t read_pin(uint8_t pin) {
    return (PINB & (1 << pin)) != 0;
}