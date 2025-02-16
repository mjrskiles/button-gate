#include "mock_hal.h"

// Example mock variables to simulate state
static bool mock_pin_state = false;
static uint32_t vmock_millis = 0;

// The mock interface instance
static HalInterface mock_hal = {
    .button_pin         = 0,
    .sig_out_pin        = 1,
    .led_mode_top_pin   = 2,
    .led_output_indicator_pin = 3,
    .led_mode_bottom_pin = 4,
    .init         = mock_hal_init,
    .set_pin      = mock_set_pin,
    .clear_pin    = mock_clear_pin,
    .toggle_pin   = mock_toggle_pin,
    .read_pin     = mock_read_pin,
    .init_timer   = mock_init_timer0,
    .millis       = mock_millis,
    .advance_time = advance_mock_time,
    .reset_time   = reset_mock_time,
};

HalInterface *p_hal = &mock_hal;

void use_mock_hal(void) {
    p_hal = &mock_hal;
}

void mock_hal_init(void) {
    // Initialize your mock state if necessary
    mock_pin_state = false;
    vmock_millis = 0;
}

void mock_set_pin(uint8_t pin) {
    mock_pin_state = true;
}

void mock_clear_pin(uint8_t pin) {
    mock_pin_state = false;
}

void mock_toggle_pin(uint8_t pin) {
    mock_pin_state = !mock_pin_state;
}

uint8_t mock_read_pin(uint8_t pin) {
    return mock_pin_state;
}

void mock_init_timer0(void) {
    // Do nothing
}

uint32_t mock_millis(void) {
    // Return the mocked time
    return vmock_millis;
}

void advance_mock_time(uint32_t ms) {
    vmock_millis += ms;
} 

void reset_mock_time(void) {
    vmock_millis = 0;
}