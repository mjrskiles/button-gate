#include "sim_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>

// Terminal state
static struct termios orig_termios;
static bool terminal_raw = false;

// Simulated hardware state
#define SIM_NUM_PINS 8
static uint8_t pin_states[SIM_NUM_PINS] = {0};

// Simulated EEPROM
#define SIM_EEPROM_SIZE 512
static uint8_t sim_eeprom[SIM_EEPROM_SIZE];

// Time tracking
static uint32_t sim_time_ms = 0;
static bool realtime_mode = true;
static bool batch_mode = false;

// Display state tracking (for change detection)
static bool display_dirty = true;
static int last_event_count = 0;

// Event log
static SimEvent event_log[SIM_EVENT_LOG_SIZE];
static int event_log_head = 0;
static int event_log_count = 0;

// Pin assignments (match mock_hal for consistency)
#define PIN_BUTTON_A    2
#define PIN_BUTTON_B    4
#define PIN_SIG_OUT     1
#define PIN_LED_TOP     5
#define PIN_LED_OUT     6
#define PIN_LED_BOT     7

// Forward declarations
static void sim_hal_init(void);
static void sim_set_pin(uint8_t pin);
static void sim_clear_pin(uint8_t pin);
static void sim_toggle_pin(uint8_t pin);
static uint8_t sim_read_pin(uint8_t pin);
static void sim_init_timer(void);
static uint32_t sim_millis(void);
static void sim_delay_ms(uint32_t ms);
static void sim_advance_time(uint32_t ms);
static void sim_reset_time(void);
static uint8_t sim_eeprom_read_byte(uint16_t addr);
static void sim_eeprom_write_byte(uint16_t addr, uint8_t value);
static uint16_t sim_eeprom_read_word(uint16_t addr);
static void sim_eeprom_write_word(uint16_t addr, uint16_t value);

// Global HAL pointer (defined in hal_interface.h as extern)
HalInterface *p_hal = NULL;

// The simulator HAL interface
static HalInterface sim_hal = {
    .max_pin            = SIM_NUM_PINS - 1,
    .button_a_pin       = PIN_BUTTON_A,
    .button_b_pin       = PIN_BUTTON_B,
    .sig_out_pin        = PIN_SIG_OUT,
    .led_mode_top_pin   = PIN_LED_TOP,
    .led_output_indicator_pin = PIN_LED_OUT,
    .led_mode_bottom_pin = PIN_LED_BOT,
    .init               = sim_hal_init,
    .set_pin            = sim_set_pin,
    .clear_pin          = sim_clear_pin,
    .toggle_pin         = sim_toggle_pin,
    .read_pin           = sim_read_pin,
    .init_timer         = sim_init_timer,
    .millis             = sim_millis,
    .delay_ms           = sim_delay_ms,
    .advance_time       = sim_advance_time,
    .reset_time         = sim_reset_time,
    .eeprom_read_byte   = sim_eeprom_read_byte,
    .eeprom_write_byte  = sim_eeprom_write_byte,
    .eeprom_read_word   = sim_eeprom_read_word,
    .eeprom_write_word  = sim_eeprom_write_word,
};

// =============================================================================
// Terminal I/O
// =============================================================================

static void enable_raw_mode(void) {
    if (terminal_raw) return;
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    terminal_raw = true;
}

static void disable_raw_mode(void) {
    if (!terminal_raw) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    terminal_raw = false;
}

// =============================================================================
// Display
// =============================================================================

static void draw_display(void) {
    // Move cursor to top-left (no clear - overwrite in place)
    printf("\033[H");

    // Header - use \033[K to clear to end of line (handles variable width content)
    printf("\033[1m=== Gatekeeper Simulator ===\033[0m              Time: %-10lu ms\033[K\n\n",
           (unsigned long)sim_time_ms);

    // Output state
    uint8_t out = pin_states[PIN_SIG_OUT];
    printf("  Output: %s\033[K\n\n",
           out ? "\033[42;30m HIGH \033[0m" : "\033[100m LOW  \033[0m");

    // Input states
    printf("  Button A: %s    Button B: %s\033[K\n\n",
           pin_states[PIN_BUTTON_A] ? "\033[43;30m[HELD]\033[0m" : "[ -- ]",
           pin_states[PIN_BUTTON_B] ? "\033[43;30m[HELD]\033[0m" : "[ -- ]");

    // Controls (static content)
    printf("\033[2m──────────────────────────────────────────────────\033[0m\033[K\n");
    printf("  [A] Button A    [B] Button B    [Q] Quit\033[K\n");
    printf("  [R] Reset time  [F] Fast/Realtime toggle\033[K\n");
    printf("\033[2m──────────────────────────────────────────────────\033[0m\033[K\n\n");

    // Mode indicator
    printf("  Mode: %-25s\033[K\n\n", realtime_mode ? "Realtime (1ms tick)" : "Fast-forward");

    // Event log
    printf("\033[1mEvent Log:\033[0m\033[K\n");
    if (event_log_count == 0) {
        printf("  \033[2m(no events yet)\033[0m\033[K\n");
        // Clear remaining log lines
        for (int i = 1; i < SIM_EVENT_LOG_SIZE; i++) {
            printf("\033[K\n");
        }
    } else {
        int start = (event_log_count < SIM_EVENT_LOG_SIZE) ? 0 :
                    (event_log_head);
        int count = (event_log_count < SIM_EVENT_LOG_SIZE) ? event_log_count : SIM_EVENT_LOG_SIZE;

        for (int i = 0; i < SIM_EVENT_LOG_SIZE; i++) {
            if (i < count) {
                int idx = (start + i) % SIM_EVENT_LOG_SIZE;
                printf("  \033[36m%8lu ms\033[0m  %-40s\033[K\n",
                       (unsigned long)event_log[idx].time_ms,
                       event_log[idx].message);
            } else {
                printf("\033[K\n");  // Clear unused log lines
            }
        }
    }

    printf("\033[K\n");
    fflush(stdout);
}

// =============================================================================
// Event logging
// =============================================================================

void sim_log_event(const char *fmt, ...) {
    SimEvent *evt = &event_log[event_log_head];
    evt->time_ms = sim_time_ms;

    va_list args;
    va_start(args, fmt);
    vsnprintf(evt->message, sizeof(evt->message), fmt, args);
    va_end(args);

    // In batch mode, print events immediately to stdout
    if (batch_mode) {
        printf("[%8lu ms] %s\n", (unsigned long)sim_time_ms, evt->message);
        fflush(stdout);
    }

    event_log_head = (event_log_head + 1) % SIM_EVENT_LOG_SIZE;
    event_log_count++;
}

// =============================================================================
// HAL Implementation
// =============================================================================

static void sim_hal_init(void) {
    memset(pin_states, 0, sizeof(pin_states));
    memset(sim_eeprom, 0xFF, sizeof(sim_eeprom));
    sim_time_ms = 0;
    sim_log_event("HAL initialized");
}

static void sim_set_pin(uint8_t pin) {
    if (pin >= SIM_NUM_PINS) return;
    if (pin_states[pin] == 0) {
        pin_states[pin] = 1;
        display_dirty = true;
        if (pin == PIN_SIG_OUT) {
            sim_log_event("Output -> HIGH");
        }
    }
}

static void sim_clear_pin(uint8_t pin) {
    if (pin >= SIM_NUM_PINS) return;
    if (pin_states[pin] == 1) {
        pin_states[pin] = 0;
        display_dirty = true;
        if (pin == PIN_SIG_OUT) {
            sim_log_event("Output -> LOW");
        }
    }
}

static void sim_toggle_pin(uint8_t pin) {
    if (pin >= SIM_NUM_PINS) return;
    pin_states[pin] = !pin_states[pin];
    display_dirty = true;
    if (pin == PIN_SIG_OUT) {
        sim_log_event("Output -> %s", pin_states[pin] ? "HIGH" : "LOW");
    }
}

static uint8_t sim_read_pin(uint8_t pin) {
    if (pin >= SIM_NUM_PINS) return 0;
    return pin_states[pin];
}

static void sim_init_timer(void) {
    // Nothing needed
}

static uint32_t sim_millis(void) {
    return sim_time_ms;
}

static void sim_delay_ms(uint32_t ms) {
    sim_time_ms += ms;
}

static void sim_advance_time(uint32_t ms) {
    sim_time_ms += ms;
}

static void sim_reset_time(void) {
    sim_time_ms = 0;
    sim_log_event("Time reset");
}

static uint8_t sim_eeprom_read_byte(uint16_t addr) {
    if (addr >= SIM_EEPROM_SIZE) return 0xFF;
    return sim_eeprom[addr];
}

static void sim_eeprom_write_byte(uint16_t addr, uint8_t value) {
    if (addr >= SIM_EEPROM_SIZE) return;
    if (sim_eeprom[addr] != value) {
        sim_eeprom[addr] = value;
    }
}

static uint16_t sim_eeprom_read_word(uint16_t addr) {
    if (addr + 1 >= SIM_EEPROM_SIZE) return 0xFFFF;
    return sim_eeprom[addr] | ((uint16_t)sim_eeprom[addr + 1] << 8);
}

static void sim_eeprom_write_word(uint16_t addr, uint16_t value) {
    if (addr + 1 >= SIM_EEPROM_SIZE) return;
    sim_eeprom[addr] = value & 0xFF;
    sim_eeprom[addr + 1] = (value >> 8) & 0xFF;
}

// =============================================================================
// Public API
// =============================================================================

void sim_init(void) {
    if (!batch_mode) {
        enable_raw_mode();
        // Hide cursor and clear screen once at startup
        printf("\033[?25l\033[2J");
    }
    // Initialize EEPROM
    memset(sim_eeprom, 0xFF, sizeof(sim_eeprom));
    sim_log_event("Simulator started");
}

void sim_cleanup(void) {
    if (!batch_mode) {
        disable_raw_mode();
        // Show cursor
        printf("\033[?25h");
        // Clear screen
        printf("\033[H\033[J");
    }
    printf("Simulator exited.\n");
}

// Called from main loop to refresh display if needed
void sim_update_display(void) {
    // No display updates in batch mode (events printed immediately)
    if (batch_mode) return;

    // Check if new events were logged
    if (event_log_count != last_event_count) {
        display_dirty = true;
        last_event_count = event_log_count;
    }

    if (display_dirty) {
        draw_display();
        display_dirty = false;
    }
}

HalInterface* sim_get_hal(void) {
    return &sim_hal;
}

void sim_set_realtime(bool realtime) {
    realtime_mode = realtime;
}

void sim_set_batch_mode(bool batch) {
    batch_mode = batch;
}

void sim_set_button_a(bool pressed) {
    if (pin_states[PIN_BUTTON_A] != pressed) {
        pin_states[PIN_BUTTON_A] = pressed;
        display_dirty = true;
    }
}

void sim_set_button_b(bool pressed) {
    if (pin_states[PIN_BUTTON_B] != pressed) {
        pin_states[PIN_BUTTON_B] = pressed;
        display_dirty = true;
    }
}

void sim_set_cv_in(bool high) {
    (void)high;  // CV input not yet implemented in sim display
    display_dirty = true;
}

bool sim_get_button_a(void) {
    return pin_states[PIN_BUTTON_A];
}

bool sim_get_button_b(void) {
    return pin_states[PIN_BUTTON_B];
}

bool sim_get_output(void) {
    return pin_states[PIN_SIG_OUT];
}

void sim_mark_dirty(void) {
    display_dirty = true;
}
