#include "hal.h"

/*
    Initializes I/O pins for this application.

    - Disables the ADC (if analog functions aren't needed).
    - Configures UART_TX and LED_PIN as outputs.
    - Sets initial states:
        * UART_TX: idle high.
        * LED_PIN: set off (adjust based on wiring: active-high or active-low).
    - For any unused pins, you might want to explicitly disable pull-ups or set them as outputs.
*/
void init_pins(void) {
    // Disable ADC to ensure that all pins function as digital I/O
    ADCSRA &= ~(1 << ADEN);

    // LED pin configuration:
    DDRB |= (1 << LED_PIN);
    
    // Configure LED off initially.
    PORTB &= ~(1 << LED_PIN);

    // For unused pins (e.g., PB1, PB2, PB4), decide whether to use pull-ups or drive them as outputs.
    // Here we disable pull-ups for inputs by driving PORT bits low (if leaving as inputs)
    // or set them as outputs in a known state.
    //
    // Example: Setting PB1, PB2, and PB4 as outputs and driving them low:
    DDRB |= (1 << PB1) | (1 << PB2) | (1 << PB3) | (1 << PB4);
    PORTB &= ~((1 << PB1) | (1 << PB2) | (1 << PB3) | (1 << PB4));
}