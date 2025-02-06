#include "startup.h"

void run_startup_sequence(void) {
    // Flash gate LED
    set_pin(LED_GATE_PIN);
    _delay_ms(STARTUP_LED_DURATION_MS);
    clear_pin(LED_GATE_PIN);
    _delay_ms(STARTUP_LED_DURATION_MS);

    // Flash pulse LED
    set_pin(LED_PULSE_PIN);
    _delay_ms(STARTUP_LED_DURATION_MS);
    clear_pin(LED_PULSE_PIN);
    _delay_ms(STARTUP_LED_DURATION_MS);

    // Flash toggle LED
    set_pin(LED_TOGGLE_PIN);
    _delay_ms(STARTUP_LED_DURATION_MS);
    clear_pin(LED_TOGGLE_PIN);
    _delay_ms(STARTUP_LED_DURATION_MS);
}