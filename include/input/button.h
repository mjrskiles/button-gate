#ifndef BUTTONGATE_INPUT_BUTTON_H
#define BUTTONGATE_INPUT_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/hal_interface.h"

typedef enum {
    MODE_GATE,
    MODE_PULSE,
    MODE_TOGGLE,
} ButtonMode;

typedef struct Button {
    uint8_t pin;
    bool pressed;
    bool last_state;
    bool rising_edge;
    uint8_t tap_count;
    uint32_t last_tap_time;
    ButtonMode mode;
} Button;

bool button_init(Button *button, uint8_t pin);
void button_update(Button *button);
ButtonMode button_get_mode(Button *button);

// Helper functions
bool is_rising_edge(bool current_state, bool last_state);

#endif // BUTTONGATE_INPUT_BUTTON_H
