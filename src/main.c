#include <avr/io.h>
#include <util/delay.h>
#include "hal.h"

int main(void) {
    init_pins();

    while (1) {
        PORTB ^= (1 << LED_PIN);  // Toggle LED pin
        _delay_ms(500);
    }

    return 0;
}