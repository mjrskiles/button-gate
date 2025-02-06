#ifndef BUTTONGATE_INPUT_BUTTON_H
#define BUTTONGATE_INPUT_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

#include "hal.h"

typedef struct Button {
    uint8_t pin;
    bool pressed;
    bool last_state;
} Button;

void init_button(Button *button, uint8_t pin);
void update_button(Button *button);

#endif // BUTTONGATE_INPUT_BUTTON_H
