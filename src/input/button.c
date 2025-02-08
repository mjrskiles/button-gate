#include "input/button.h"

#define TAP_TIMEOUT_MS 500    // Maximum time between taps
#define TAPS_TO_CHANGE 5      // Number of taps needed to change mode

void init_button(Button *button, uint8_t pin) {
    button->pin = pin;
    button->pressed = false;
    button->last_state = false;
    button->tap_count = 0;
    button->last_tap_time = 0;
    button->mode = MODE_GATE;
}

static bool is_rising_edge(bool current_state, bool last_state) {
    return current_state && !last_state;
}

void update_button(Button *button) {
    bool current_state = read_pin(button->pin);
    uint32_t current_time = millis();
    
    // Detect rising edge (button press)
    if (is_rising_edge(current_state, button->last_state)) {
        // If it's been too long since last tap, reset counter
        if (current_time - button->last_tap_time > TAP_TIMEOUT_MS) {
            button->tap_count = 0;
        }
        
        button->tap_count++;
        button->last_tap_time = current_time;
        
        // Check if we've reached the required number of taps
        if (button->tap_count >= TAPS_TO_CHANGE) {
            // Change to next mode
            button->mode = (button->mode + 1) % MODE_COUNT;
            button->tap_count = 0;  // Reset tap count
        }
    }
    
    // Normal button state update
    button->last_state = current_state;
    button->pressed = current_state && !button->pressed;
    
    // Reset tap count if timeout occurred
    if (current_time - button->last_tap_time > TAP_TIMEOUT_MS) {
        button->tap_count = 0;
    }
}

ButtonMode get_button_mode(Button *button) {
    return button->mode;
}