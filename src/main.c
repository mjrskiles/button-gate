#include <avr/io.h>
#include "app_init.h"
#include "hardware/hal_interface.h"
#include "controller/io_controller.h"

int main(void) {
    // Initialize hardware
    p_hal->init();

    // Run app initialization - handles factory reset detection and settings loading
    AppSettings settings;
    AppInitResult init_result = app_init_run(&settings);

    // TODO: Pass settings.mode to io_controller when it supports initial mode
    (void)init_result;  // Unused for now

    // Initialize button (primary button only for now)
    Button button1;
    button_init(&button1, p_hal->button_a_pin);

    // Initialize CV output
    CVOutput cv_output;
    cv_output_init(&cv_output, p_hal->sig_out_pin);

    // Initialize IO controller
    IOController io_controller;
    io_controller_init(&io_controller, &button1, &cv_output, p_hal->led_output_indicator_pin);

    // Main loop
    while (1) {
        io_controller_update(&io_controller);
    }

    return 0;
}