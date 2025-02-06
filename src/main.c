#include <avr/io.h>
#include <util/delay.h>
#include "hal.h"    
#include "startup.h"
#include "input/button.h"

int main(void) {

    // Set IO pins as inputs/outputs and init to default state
    init_pins();

    // Flash LEDs to test startup sequence
    run_startup_sequence();

    // Initialize buttons
    Button button1;
    init_button(&button1, BUTTON_PIN);

    set_pin(LED_GATE_PIN);

    while (1) {
        update_button(&button1);
        if (button1.pressed) {
            // set the signal out pin high
            set_pin(SIG_OUT_PIN);
        } else {
            // set the signal out pin low
            clear_pin(SIG_OUT_PIN);
        }
    }

    return 0;
}