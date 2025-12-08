#ifndef GK_HARDWARE_HAL_INTERFACE_H
#define GK_HARDWARE_HAL_INTERFACE_H

#include <stdint.h>

/**
 * Hardware Abstraction Layer (HAL) Interface
 *
 * This interface decouples application logic from hardware-specific code,
 * enabling unit testing on the host machine without actual hardware.
 *
 * Architecture:
 * - A global pointer `p_hal` points to the active HAL implementation
 * - Production builds use the real HAL (hal.c) targeting ATtiny85
 * - Test builds swap in a mock HAL (mock_hal.c) with virtual time
 *
 * Usage in tests:
 *   p_hal points to mock_hal by default in test builds.
 *   Use p_hal->advance_time(ms) to simulate time passing.
 *   Use p_hal->reset_time() in test teardown for isolation.
 *
 * Why a global?
 *   On memory-constrained MCUs (512 bytes SRAM), passing HAL pointers
 *   to every function wastes stack space. A single global is the
 *   standard embedded pattern and works well with the mock swap approach.
 */
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
    void     (*init_timer)(void);
    uint32_t (*millis)(void);
    void     (*advance_time)(uint32_t ms);
    void     (*reset_time)(void);
} HalInterface;

// Global pointer to the current HAL implementation.
// This pointer defaults to the production HAL, but tests can replace it with a mock.
extern HalInterface *p_hal;

#endif /* GK_HARDWARE_HAL_INTERFACE_H */