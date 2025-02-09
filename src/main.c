#include <avr/io.h>
#include <util/delay.h>
#include "hardware/hal_interface.h"    
#include "startup.h"
#include "input/button.h"

int main(void) {

    // Set IO pins as inputs/outputs and init to default state
    p_hal->init();

    // Flash LEDs to test startup sequence
    run_startup_sequence();

    // Initialize button
    Button button1;
    button_init(&button1, BUTTON_PIN);

    p_hal->set_pin(LED_MODE_TOP_PIN);

    p_hal->set_pin(SIG_OUT_PIN);

    while (1) {
        button_update(&button1);

        if (button1.pressed) {
            p_hal->set_pin(SIG_OUT_PIN);
        } else {
            p_hal->clear_pin(SIG_OUT_PIN);
        }
    }

    return 0;
}