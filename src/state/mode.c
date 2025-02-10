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
