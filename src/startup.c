#include "startup.h"

void run_startup_sequence(void) {
    // Flash top LED
    p_hal->set_pin(p_hal->led_mode_top_pin);
    util_delay_ms(STARTUP_LED_DURATION_MS);
    p_hal->clear_pin(p_hal->led_mode_top_pin);
    util_delay_ms(STARTUP_LED_DURATION_MS);

    // Flash middle LED
    p_hal->set_pin(p_hal->led_mode_bottom_pin);
    util_delay_ms(STARTUP_LED_DURATION_MS);
    p_hal->clear_pin(p_hal->led_mode_bottom_pin);
    util_delay_ms(STARTUP_LED_DURATION_MS);

    // Flash bottom LED
    p_hal->set_pin(p_hal->led_output_indicator_pin);
    util_delay_ms(STARTUP_LED_DURATION_MS);
    p_hal->clear_pin(p_hal->led_output_indicator_pin);
    util_delay_ms(STARTUP_LED_DURATION_MS);
}