#ifndef GK_STATE_MODE_H
#define GK_STATE_MODE_H

#include <stdbool.h>
#include "core/states.h"

/**
 * @file mode.h
 * @brief Mode definitions and utilities
 *
 * This file provides the CVMode type alias and LED state utilities.
 * The actual mode enum is defined in fsm/gatekeeper_states.h as ModeState.
 *
 * Note: MODE_PULSE is a legacy alias for MODE_TRIGGER
 */

// Type alias for backward compatibility
typedef ModeState CVMode;

// Legacy alias: PULSE -> TRIGGER
#define MODE_PULSE MODE_TRIGGER

/**
 * Mode LED indicator pattern
 * Two LEDs encode the current mode in binary:
 *   Gate:    top=ON,  bottom=OFF  (binary: 01)
 *   Trigger: top=OFF, bottom=ON   (binary: 10)
 *   Toggle:  top=ON,  bottom=ON   (binary: 11)
 *   Divide:  top=OFF, bottom=OFF  (binary: 00) + blink pattern
 *   Cycle:   (handled by Neopixel in AP-004)
 */
typedef struct ModeLEDState {
    bool top;
    bool bottom;
} ModeLEDState;

/**
 * Get the next mode in sequence (ring navigation).
 *
 * @param mode  Current mode
 * @return      Next mode
 */
CVMode cv_mode_get_next(CVMode mode);

/**
 * Get LED state for a given mode.
 *
 * @param mode  Current mode
 * @return      LED state (top/bottom)
 */
ModeLEDState cv_mode_get_led_state(CVMode mode);

#endif /* GK_STATE_MODE_H */
