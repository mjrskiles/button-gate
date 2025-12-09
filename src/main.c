#include <avr/io.h>
#include "app_init.h"
#include "hardware/hal_interface.h"
#include "core/coordinator.h"

static Coordinator coordinator;
static AppSettings settings;

int main(void) {
    // Initialize hardware
    p_hal->init();

    // Enable watchdog timer (250ms timeout)
    p_hal->wdt_enable();

    // Run app initialization - handles factory reset detection and settings loading
    AppInitResult init_result = app_init_run(&settings);
    (void)init_result;  // Could log or indicate via LED

    // Initialize coordinator
    coordinator_init(&coordinator, &settings);

    // Restore mode from saved settings
    if (settings.mode < MODE_COUNT) {
        coordinator_set_mode(&coordinator, (ModeState)settings.mode);
    }

    // Start coordinator
    coordinator_start(&coordinator);

    // Main loop
    while (1) {
        // Feed watchdog at start of each loop iteration
        p_hal->wdt_reset();

        // Update coordinator (processes inputs, runs mode handlers)
        coordinator_update(&coordinator);

        // Update output pin based on coordinator output state
        if (coordinator_get_output(&coordinator)) {
            p_hal->set_pin(p_hal->sig_out_pin);
            p_hal->set_pin(p_hal->led_output_indicator_pin);
        } else {
            p_hal->clear_pin(p_hal->sig_out_pin);
            p_hal->clear_pin(p_hal->led_output_indicator_pin);
        }
    }

    return 0;
}
