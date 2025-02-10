#ifndef HAL_INTERFACE_H
#define HAL_INTERFACE_H

#include <stdint.h>

typedef struct HalInterface {
    // pin assignment
    uint8_t  button_pin;
    uint8_t  sig_out_pin;
    uint8_t  led_mode_top_pin;
    uint8_t  led_output_indicator_pin;
    uint8_t  led_mode_bottom_pin;

    // IO functions
    void     (*init)(void);
    void     (*set_pin)(uint8_t pin);
    void     (*clear_pin)(uint8_t pin);
    void     (*toggle_pin)(uint8_t pin);
    uint8_t  (*read_pin)(uint8_t pin);

    // Timer functions
    void     (*init_timer0)(void);
    uint32_t (*millis)(void);
} HalInterface;

// Global pointer to the current HAL implementation.
// This pointer defaults to the production HAL, but tests can replace it with a mock.
extern HalInterface *p_hal;

#endif // HAL_INTERFACE_H