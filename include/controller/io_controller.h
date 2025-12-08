#ifndef GK_CONTROLLER_IO_CONTROLLER_H
#define GK_CONTROLLER_IO_CONTROLLER_H

#include "stdint.h"

#include "hardware/hal_interface.h"
#include "input/button.h"
#include "output/cv_output.h"
#include "state/mode.h"

typedef struct IOController {
    Button *button;
    CVOutput *cv_output; 
    uint8_t led_pin;
    CVMode mode;
    bool ignore_pressed;
} IOController;

void io_controller_init(IOController *io_controller, Button *button, CVOutput *cv_output, uint8_t led_pin);

void io_controller_update(IOController *io_controller);

#endif /* GK_CONTROLLER_IO_CONTROLLER_H */
