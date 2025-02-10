#include "output/cv_output.h"

void cv_output_init(CVOutput *cv_output, uint8_t pin) {
    cv_output->pin = pin;
    cv_output->state = false;
}

void cv_output_reset(CVOutput *cv_output) {
    cv_output_clear(cv_output);
}

void cv_output_set(CVOutput *cv_output) {
    cv_output->state = true;
    p_hal->set_pin(cv_output->pin);

}

void cv_output_clear(CVOutput *cv_output) {
    cv_output->state = false;
    p_hal->clear_pin(cv_output->pin);
}

bool cv_output_update_gate(CVOutput *cv_output, bool input_state) {
    if (input_state) {
        cv_output_set(cv_output);
    } else {
        cv_output_clear(cv_output);
    }
    return cv_output->state;
}

bool cv_output_update_pulse(CVOutput *cv_output, bool input_state) {
    static uint32_t pulse_start_time = 0;
    static bool pulse_active = false;
    static bool last_input_state = false;

    if (input_state && !last_input_state) {
        pulse_start_time = p_hal->millis();
        pulse_active = true;
        cv_output_set(cv_output);
    }

    if (pulse_active) {
        uint32_t current_time = p_hal->millis();
        if (current_time - pulse_start_time >= PULSE_DURATION_MS) {
            pulse_active = false;
            cv_output_clear(cv_output);
        }
    }

    last_input_state = input_state;
    return cv_output->state;
}

bool cv_output_update_toggle(CVOutput *cv_output, bool input_state) {
    static bool last_input_state = false;

    // Only toggle on rising edge (false -> true transition)
    if (input_state && !last_input_state) {
        cv_output->state = !cv_output->state;
        if (cv_output->state) {
            cv_output_set(cv_output);
        } else {
            cv_output_clear(cv_output);
        }
    }

    last_input_state = input_state;
    return cv_output->state;
}
