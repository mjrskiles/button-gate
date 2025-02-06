#ifndef HAL_H
#define HAL_H

#include <avr/io.h>

#define BUTTON_PIN PB0
#define SIG_OUT_PIN PB1
#define LED_GATE_PIN PB2
#define LED_TOGGLE_PIN PB3
#define LED_PULSE_PIN PB4

void init_pins(void);

void set_pin(uint8_t pin);
void clear_pin(uint8_t pin);
void toggle_pin(uint8_t pin);

uint8_t read_pin(uint8_t pin);

#endif


