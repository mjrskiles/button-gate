#include "input/button.h"

bool button_init(Button *button, uint8_t pin) {
    button->pin = pin;
    button->config_action = false;
    button_reset(button);

    return true;
}

void button_reset(Button *button) {
    button->raw_state = false;
    button->pressed = false;
    button->last_state = false;
    button->rising_edge = false;
    button->falling_edge = false;
    button->tap_count = 0;
    button->last_rise_time = 0;
    button->last_fall_time = 0;
}

bool button_has_rising_edge(Button *button) {
    // If the button is pressed and was not pressed before, it's a rising edge.
    // Take into account debounce time.
    if (button->raw_state && !button->last_state) {
        if (p_hal->millis() - button->last_rise_time > EDGE_DEBOUNCE_MS) {
            button->last_rise_time = p_hal->millis();
            return true;
        }
    }
    return false;
}

bool button_has_falling_edge(Button *button) {
    // If the button is not pressed and was pressed before, it's a falling edge.
    if (!button->raw_state && button->last_state) {
        if (p_hal->millis() - button->last_fall_time > EDGE_DEBOUNCE_MS) {
            button->last_fall_time = p_hal->millis();
            return true;
        }
    }
    return false;
}

void button_update(Button *button) {
    button->raw_state = p_hal->read_pin(button->pin);
    button->rising_edge = false;
    button->falling_edge = false;
    
    // Detect rising edge (button press)
    if (button_has_rising_edge(button)) {
        button->rising_edge = true;
        button->pressed = true;
    }

    // Detect falling edge (button release)
    if (button_has_falling_edge(button)) {
        button->falling_edge = true;
        button->pressed = false;
    }

    // Check if we've requested a mode change
    if (button_detect_config_action(button)) {
        // Set config action flag
        button->config_action = true;
    }

    button->last_state = button->pressed;
}

void button_consume_config_action(Button *button) {
    button->config_action = false;
}

bool button_detect_config_action(Button *button) {
    uint32_t current_time = p_hal->millis();
    static uint32_t last_tap_time = 0;
    static bool counting_hold = false;
    bool action_detected = false;

    // On rising edge (new press)
    if (button->rising_edge) {
        // Check if this tap is within the timeout window
        if (current_time - last_tap_time <= TAP_TIMEOUT_MS) {
            button->tap_count++;
            
            // If we've reached required taps, start counting hold time
            if (button->tap_count >= TAPS_TO_CHANGE) {
                counting_hold = true;
            }
        } else {
            // Too much time passed, reset tap count
            button->tap_count = 1;
        }
        last_tap_time = current_time;
    }

    // Check for hold after required taps
    if (counting_hold && button->pressed) {
        if (current_time - last_tap_time >= HOLD_TIME_MS) {
            action_detected = true;
            counting_hold = false;
            button->tap_count = 0;
        }
    }

    // Reset if button released
    if (!button->pressed) {
        counting_hold = false;
        if (current_time - last_tap_time > TAP_TIMEOUT_MS) {
            button->tap_count = 0;
        }
    }

    return action_detected;
}