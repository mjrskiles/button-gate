#include "controller/io_controller.h"

void io_controller_init(IOController *io_controller, Button *button, CVOutput *cv_output, uint8_t led_pin) {
    io_controller->button = button;
    io_controller->cv_output = cv_output;
    io_controller->led_pin = led_pin;
}

void io_controller_update(IOController *io_controller) {
    button_update(io_controller->button);

    if (io_controller->button->config_action) {
        io_controller->mode = cv_mode_get_next(io_controller->mode);
        button_consume_config_action(io_controller->button);
        button_reset(io_controller->button);
        cv_output_reset(io_controller->cv_output);
    }

    switch (io_controller->mode) {
        case MODE_GATE:
            cv_output_update_gate(io_controller->cv_output, io_controller->button->pressed);
            p_hal->set_pin(p_hal->led_mode_top_pin);
            p_hal->clear_pin(p_hal->led_mode_bottom_pin);
            break;
        case MODE_PULSE:
            cv_output_update_pulse(io_controller->cv_output, io_controller->button->pressed);
            p_hal->clear_pin(p_hal->led_mode_top_pin);
            p_hal->set_pin(p_hal->led_mode_bottom_pin);
            break;
        case MODE_TOGGLE:
            cv_output_update_toggle(io_controller->cv_output, io_controller->button->pressed);
            p_hal->set_pin(p_hal->led_mode_top_pin);
            p_hal->set_pin(p_hal->led_mode_bottom_pin);
            break;
        default:
            cv_output_update_gate(io_controller->cv_output, io_controller->button->pressed);
            p_hal->set_pin(p_hal->led_mode_top_pin);
            p_hal->clear_pin(p_hal->led_mode_bottom_pin);
            break;
    }

    if (io_controller->cv_output->state) {
        p_hal->set_pin(io_controller->led_pin);
    } else {
        p_hal->clear_pin(io_controller->led_pin);
    }
}