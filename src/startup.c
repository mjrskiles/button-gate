#include "startup.h"

void run_startup_sequence(void) {
    // Flash gate LED
    hal_set_pin(LED_MODE_TOP_PIN);
    _delay_ms(STARTUP_LED_DURATION_MS);
    hal_clear_pin(LED_MODE_TOP_PIN);
    _delay_ms(STARTUP_LED_DURATION_MS);

    // Flash pulse LED
    hal_set_pin(LED_MODE_BOTTOM_PIN);
    _delay_ms(STARTUP_LED_DURATION_MS);
    hal_clear_pin(LED_MODE_BOTTOM_PIN);
    _delay_ms(STARTUP_LED_DURATION_MS);

    // Flash toggle LED
    hal_set_pin(LED_OUTPUT_INDICATOR_PIN);
    _delay_ms(STARTUP_LED_DURATION_MS);
    hal_clear_pin(LED_OUTPUT_INDICATOR_PIN);
    _delay_ms(STARTUP_LED_DURATION_MS);
}