#ifndef BUTTONGATE_INPUT_BUTTON_H
#define BUTTONGATE_INPUT_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

#include "hal.h"

typedef enum {
    MODE_GATE,
    MODE_PULSE,
    MODE_TOGGLE,
} ButtonMode;

typedef struct Button {
    uint8_t pin;
    bool pressed;
    bool last_state;
    uint8_t tap_count;
    uint32_t last_tap_time;
    ButtonMode mode;
} Button;

void init_button(Button *button, uint8_t pin);
static bool is_rising_edge(bool current_state, bool last_state);
void update_button(Button *button);
ButtonMode get_button_mode(Button *button);

#endif // BUTTONGATE_INPUT_BUTTON_H
