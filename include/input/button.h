#ifndef GK_INPUT_BUTTON_H
#define GK_INPUT_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/hal_interface.h"

#define TAP_TIMEOUT_MS 500
#define TAPS_TO_CHANGE 5
#define HOLD_TIME_MS 1000
#define EDGE_DEBOUNCE_MS 5

typedef struct Button {
    uint8_t pin;    // Pin number
    bool raw_state; // Raw state of the button
    bool pressed;   // True if the button is pressed. This is debounced and represents a real user press.
    bool last_state; // Last state of the button.
    bool rising_edge; // True if the button has a rising edge
    bool falling_edge; // True if the button has a falling edge
    bool config_action; // True if the button has detected a config action
    uint8_t tap_count; // Number of taps on the button (for config action)
    uint32_t last_rise_time; // Last time the button had a rising edge
    uint32_t last_fall_time; // Last time the button had a falling edge

    // Config action detection state
    uint32_t last_tap_time; // Last time a tap was registered
    bool counting_hold; // True if we're counting hold time after reaching required taps
} Button;

bool button_init(Button *button, uint8_t pin);

/*
    Reset the button to its initial state
*/
void button_reset(Button *button);

void button_update(Button *button);

/*
    Consume the config action flag
    Call this when you're ready to handle the config action
    This will reset the config action flag
*/
void button_consume_config_action(Button *button);

/*
    Detect if a config action has occurred
    The config action is executed by tapping the button
    5 times in a row and holding for 1 second on the last tap
    Returns true if a config action has occurred
*/
bool button_detect_config_action(Button *button);

// Helper functions
bool button_has_rising_edge(Button *button);
bool button_has_falling_edge(Button *button);


#endif /* GK_INPUT_BUTTON_H */
