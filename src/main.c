#include <avr/io.h>
#include <util/delay.h>  
#include "startup.h"
#include "hardware/hal_interface.h"  
#include "controller/io_controller.h"

int main(void) {

    // Set IO pins as inputs/outputs and init to default state
    p_hal->init();

    // Flash LEDs to test startup sequence
    run_startup_sequence();

    // Initialize button
    Button button1;
    button_init(&button1, p_hal->button_pin);

    // Initialize CV output
    CVOutput cv_output;
    cv_output_init(&cv_output, p_hal->sig_out_pin);

    // Initialize IO controller
    IOController io_controller;
    io_controller_init(&io_controller, &button1, &cv_output, p_hal->led_output_indicator_pin);

    while (1) {
        io_controller_update(&io_controller);
    }

    return 0;
}