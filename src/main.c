#include <avr/io.h>
#include "app_init.h"
#include "hardware/hal_interface.h"
#include "core/coordinator.h"
#include "output/led_feedback.h"

static Coordinator coordinator;
static AppSettings settings;
static LEDFeedbackController led_ctrl;

int main(void) {
    // Initialize hardware
    p_hal->init();

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

    // Initialize LED feedback controller
    led_feedback_init(&led_ctrl);
    led_feedback_set_mode(&led_ctrl, coordinator_get_mode(&coordinator));

    // Enable watchdog timer (250ms timeout) after init complete
    p_hal->wdt_enable();

    // Main loop
    while (1) {
        // Feed watchdog at start of each loop iteration
        p_hal->wdt_reset();

        // Update coordinator (processes inputs, runs mode handlers)
        coordinator_update(&coordinator);

        // Update LED feedback
        LEDFeedback feedback;
        coordinator_get_led_feedback(&coordinator, &feedback);
        led_feedback_update(&led_ctrl, &feedback, p_hal->millis());

        // Update output pin based on coordinator output state
        // Note: Output LED is driven in-circuit from the signal output buffer
        if (coordinator_get_output(&coordinator)) {
            p_hal->set_pin(p_hal->sig_out_pin);
        } else {
            p_hal->clear_pin(p_hal->sig_out_pin);
        }
    }

    return 0;
}
