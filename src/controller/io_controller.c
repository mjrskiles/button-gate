#include "controller/io_controller.h"
#include "utility/status.h"

void io_controller_init(IOController *io_controller, Button *button, CVOutput *cv_output, uint8_t led_pin) {
    if (!io_controller || !button || !cv_output) return;

    io_controller->button = button;
    io_controller->mode = MODE_GATE;
    io_controller->cv_output = cv_output;
    io_controller->led_pin = led_pin;
    io_controller->ignore_pressed = false;
}

void io_controller_update(IOController *io_controller) {
    button_update(io_controller->button);

    if (STATUS_ANY(io_controller->button->status, BTN_CONFIG)) {
        io_controller->mode = cv_mode_get_next(io_controller->mode);
        io_controller->ignore_pressed = true;
        button_consume_config_action(io_controller->button);
        cv_output_reset(io_controller->cv_output);
    }

    bool input_triggered = io_controller->ignore_pressed
        ? false
        : STATUS_ANY(io_controller->button->status, BTN_PRESSED);

    switch (io_controller->mode) {
        case MODE_GATE:
            cv_output_update_gate(io_controller->cv_output, input_triggered);
            break;
        case MODE_PULSE:
            cv_output_update_pulse(io_controller->cv_output, input_triggered);
            break;
        case MODE_TOGGLE:
            cv_output_update_toggle(io_controller->cv_output, input_triggered);
            break;
        default:
            cv_output_update_gate(io_controller->cv_output, input_triggered);
            break;
    }

    // Update mode indicator LEDs
    ModeLEDState led_state = cv_mode_get_led_state(io_controller->mode);
    if (led_state.top) {
        p_hal->set_pin(p_hal->led_mode_top_pin);
    } else {
        p_hal->clear_pin(p_hal->led_mode_top_pin);
    }
    if (led_state.bottom) {
        p_hal->set_pin(p_hal->led_mode_bottom_pin);
    } else {
        p_hal->clear_pin(p_hal->led_mode_bottom_pin);
    }

    if (STATUS_ANY(io_controller->cv_output->status, CVOUT_STATE)) {
        p_hal->set_pin(io_controller->led_pin);
    } else {
        p_hal->clear_pin(io_controller->led_pin);
    }

    if (STATUS_ANY(io_controller->button->status, BTN_FALL)) {
        io_controller->ignore_pressed = false;
    }
}