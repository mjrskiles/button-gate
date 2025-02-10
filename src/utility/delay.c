#include "utility/delay.h"

void util_delay_ms(uint32_t ms) {
    uint32_t start_time = p_hal->millis();
    while (p_hal->millis() - start_time < ms) {
        // Busy-wait
    }
}