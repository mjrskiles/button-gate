#include "state/mode.h"

CVMode cv_mode_get_next(CVMode mode) {
    // Ring navigation through all modes
    return (CVMode)((mode + 1) % MODE_COUNT);
}

ModeLEDState cv_mode_get_led_state(CVMode mode) {
    ModeLEDState state = { .top = false, .bottom = false };

    switch (mode) {
        case MODE_GATE:
            state.top = true;
            state.bottom = false;
            break;
        case MODE_TRIGGER:
            state.top = false;
            state.bottom = true;
            break;
        case MODE_TOGGLE:
            state.top = true;
            state.bottom = true;
            break;
        case MODE_DIVIDE:
            // Both off, distinguished by blink pattern or Neopixel
            state.top = false;
            state.bottom = false;
            break;
        case MODE_CYCLE:
            // Handled by Neopixel (AP-004)
            // For now, show distinct pattern
            state.top = false;
            state.bottom = false;
            break;
        default:
            state.top = true;
            state.bottom = false;
            break;
    }
    return state;
}
