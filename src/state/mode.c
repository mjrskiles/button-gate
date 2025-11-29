#include "state/mode.h"

CVMode cv_mode_get_next(CVMode mode) {
    switch (mode) {
        case MODE_GATE:
            return MODE_PULSE;
        case MODE_PULSE:
            return MODE_TOGGLE;
        case MODE_TOGGLE:
            return MODE_GATE;
        default:
            return MODE_GATE;
    }
}

ModeLEDState cv_mode_get_led_state(CVMode mode) {
    ModeLEDState state;
    switch (mode) {
        case MODE_GATE:
            state.top = true;
            state.bottom = false;
            break;
        case MODE_PULSE:
            state.top = false;
            state.bottom = true;
            break;
        case MODE_TOGGLE:
            state.top = true;
            state.bottom = true;
            break;
        default:
            state.top = true;
            state.bottom = false;
            break;
    }
    return state;
}
