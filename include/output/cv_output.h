#ifndef CV_OUTPUT_H
#define CV_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/hal_interface.h"

#define PULSE_DURATION_MS 10

typedef struct CVOutput {
    uint8_t pin;
    bool state;
} CVOutput;

void cv_output_init(CVOutput *cv_output, uint8_t pin);

void cv_output_set(CVOutput *cv_output);

void cv_output_clear(CVOutput *cv_output);

bool cv_output_update_gate(CVOutput *cv_output, bool input_state);

bool cv_output_update_pulse(CVOutput *cv_output, bool input_state);

bool cv_output_update_toggle(CVOutput *cv_output, bool input_state);


#endif
