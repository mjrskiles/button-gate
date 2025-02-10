#ifndef CV_MODE_H
#define CV_MODE_H

typedef enum {
    MODE_GATE,
    MODE_PULSE,
    MODE_TOGGLE,
} CVMode;

CVMode cv_mode_get_next(CVMode mode);


#endif