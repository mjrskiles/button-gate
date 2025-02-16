#ifndef HAL_H
#define HAL_H

#include <avr/io.h>
#include <avr/interrupt.h>

#include <stdint.h>

#include "hardware/hal_interface.h"

#define BUTTON_PIN PB0
#define SIG_OUT_PIN PB1
#define LED_MODE_TOP_PIN PB2
#define LED_OUTPUT_INDICATOR_PIN PB3
#define LED_MODE_BOTTOM_PIN PB4

void hal_init(void);

void hal_set_pin(uint8_t pin);
void hal_clear_pin(uint8_t pin);
void hal_toggle_pin(uint8_t pin);

uint8_t hal_read_pin(uint8_t pin);

void hal_init_timer0(void);
uint32_t hal_millis(void);
void hal_advance_time(uint32_t ms);
void hal_reset_time(void);

#endif


