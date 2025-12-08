#ifndef GK_STATE_MODE_H
#define GK_STATE_MODE_H

#include <stdbool.h>

typedef enum {
    MODE_GATE,
    MODE_PULSE,
    MODE_TOGGLE,
} CVMode;

/**
 * Mode LED indicator pattern
 * Two LEDs encode the current mode in binary:
 *   Gate:   top=ON,  bottom=OFF
 *   Pulse:  top=OFF, bottom=ON
 *   Toggle: top=ON,  bottom=ON
 */
typedef struct ModeLEDState {
    bool top;
    bool bottom;
} ModeLEDState;

CVMode cv_mode_get_next(CVMode mode);

ModeLEDState cv_mode_get_led_state(CVMode mode);

#endif /* GK_STATE_MODE_H */