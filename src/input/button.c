#include "input/button.h"

void init_button(Button *button, uint8_t pin) {
    button->pin = pin;
    button->pressed = false;
    button->last_state = false;
}

void update_button(Button *button) {
    button->last_state = read_pin(button->pin);
    button->pressed = button->last_state && !button->pressed;
}