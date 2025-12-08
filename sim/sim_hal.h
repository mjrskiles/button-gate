#ifndef GK_SIM_HAL_H
#define GK_SIM_HAL_H

#include "hardware/hal_interface.h"
#include <stdbool.h>

/**
 * @file sim_hal.h
 * @brief x86 simulator HAL with terminal I/O
 *
 * Provides an interactive terminal-based HAL for testing
 * application logic without hardware.
 */

// Event log entry
typedef struct {
    uint32_t time_ms;
    char message[48];
} SimEvent;

#define SIM_EVENT_LOG_SIZE 8

/**
 * Initialize simulator (terminal setup, etc.)
 * Must be called before using the sim HAL.
 */
void sim_init(void);

/**
 * Cleanup simulator (restore terminal settings)
 * Must be called before exit.
 */
void sim_cleanup(void);

/**
 * Update display if dirty.
 * Call periodically from main loop.
 */
void sim_update_display(void);

/**
 * Get the simulator HAL interface.
 * Assign to p_hal before running app code.
 */
HalInterface* sim_get_hal(void);

/**
 * Log an event to the event history.
 * Called automatically by sim HAL on state changes.
 * Can also be called manually for custom events.
 */
void sim_log_event(const char *fmt, ...);

/**
 * Configuration options
 */
void sim_set_realtime(bool realtime);  // true = 1ms ticks, false = fast-forward
void sim_set_batch_mode(bool batch);   // true = no ANSI UI, plain text output

/**
 * Input state accessors (for input sources)
 * These set the simulated hardware input state.
 */
void sim_set_button_a(bool pressed);
void sim_set_button_b(bool pressed);
void sim_set_cv_in(bool high);

bool sim_get_button_a(void);
bool sim_get_button_b(void);
bool sim_get_output(void);

/**
 * Mark display as needing refresh
 */
void sim_mark_dirty(void);

#endif /* GK_SIM_HAL_H */
