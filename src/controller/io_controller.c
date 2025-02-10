#include "controller/io_controller.h"

void io_controller_init(IOController *io_controller, Button *button, CVOutput *cv_output, uint8_t led_pin) {
    io_controller->button = button;
    io_controller->mode = MODE_GATE;
    io_controller->cv_output = cv_output;
    io_controller->led_pin = led_pin;
    io_controller->ignore_pressed = false;
}

void io_controller_update(IOController *io_controller) {
    button_update(io_controller->button);

    if (io_controller->button->config_action) {
        io_controller->mode = cv_mode_get_next(io_controller->mode);
        io_controller->ignore_pressed = true;
        button_consume_config_action(io_controller->button);
        cv_output_reset(io_controller->cv_output);
    }

    bool input_triggered = io_controller->ignore_pressed ? false : io_controller->button->pressed;

    switch (io_controller->mode) {
        case MODE_GATE:
            cv_output_update_gate(io_controller->cv_output, input_triggered);
            p_hal->set_pin(p_hal->led_mode_top_pin);
            p_hal->clear_pin(p_hal->led_mode_bottom_pin);
            break;
        case MODE_PULSE:
            cv_output_update_pulse(io_controller->cv_output, input_triggered);
            p_hal->clear_pin(p_hal->led_mode_top_pin);
            p_hal->set_pin(p_hal->led_mode_bottom_pin);
            break;
        case MODE_TOGGLE:
            cv_output_update_toggle(io_controller->cv_output, input_triggered);
            p_hal->set_pin(p_hal->led_mode_top_pin);
            p_hal->set_pin(p_hal->led_mode_bottom_pin);
            break;
        default:
            cv_output_update_gate(io_controller->cv_output, input_triggered);
            p_hal->set_pin(p_hal->led_mode_top_pin);
            p_hal->clear_pin(p_hal->led_mode_bottom_pin);
            break;
    }

    if (io_controller->cv_output->state) {
        p_hal->set_pin(io_controller->led_pin);
    } else {
        p_hal->clear_pin(io_controller->led_pin);
    }

    if (io_controller->button->falling_edge) {
        io_controller->ignore_pressed = false;
    }
}