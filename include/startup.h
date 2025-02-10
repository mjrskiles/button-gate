#ifndef BUTTONGATE_STARTUP_H
#define BUTTONGATE_STARTUP_H

#include "hardware/hal_interface.h"
#include "utility/delay.h"

// Duration for each LED flash in milliseconds
#define STARTUP_LED_DURATION_MS 200

/**
 * Flashes each LED in sequence as a startup test pattern.
 * This helps verify all LEDs are working properly on power-up.
 */
void run_startup_sequence(void);


#endif // BUTTONGATE_STARTUP_H
