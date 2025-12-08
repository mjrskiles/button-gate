#include "sim_hal.h"
#include "input_source.h"
#include "hardware/hal_interface.h"
#include "app_init.h"
#include "core/coordinator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static volatile bool running = true;
static InputSource *input_source = NULL;

static void handle_signal(int sig) {
    (void)sig;
    running = false;
}

static void print_usage(const char *progname) {
    printf("Gatekeeper x86 Simulator\n\n");
    printf("Usage: %s [options]\n\n", progname);
    printf("Options:\n");
    printf("  --script <file>  Run script file instead of interactive mode\n");
    printf("  --batch          Batch mode: plain text output, no ANSI UI (for CI/agents)\n");
    printf("  --fast           Run in fast-forward mode (interactive only)\n");
    printf("  --help           Show this help message\n");
    printf("\n");
    printf("Interactive Controls:\n");
    printf("  A          Toggle Button A\n");
    printf("  B          Toggle Button B\n");
    printf("  R          Reset time\n");
    printf("  F          Toggle fast/realtime mode\n");
    printf("  Q / ESC    Quit\n");
    printf("\n");
    printf("Script Format:\n");
    printf("  # Comment\n");
    printf("  <delay_ms> <action> [target] [value]\n");
    printf("  @<abs_ms>  <action> [target] [value]   (@ = absolute time)\n");
    printf("\n");
    printf("Actions: press, release, assert, log, quit\n");
    printf("Targets: a, b, cv, output\n");
    printf("\n");
    printf("Example script:\n");
    printf("  0       press   a\n");
    printf("  100     release a\n");
    printf("  50      assert  output high\n");
    printf("  @500    quit\n");
    printf("\n");
}

// Simplified main loop tick - just advances time and updates display
static void sim_tick(void) {
    // Advance time through HAL so scripts can track it
    p_hal->advance_time(1);

    // Real-time pacing if input source requests it
    if (input_source && input_source->is_realtime(input_source)) {
        usleep(1000);  // 1ms
    }

    // Update display periodically (skip in batch mode)
    static uint32_t last_draw = 0;
    uint32_t now = p_hal->millis();
    uint32_t interval = (input_source && input_source->is_realtime(input_source)) ? 200 : 1000;
    if (now - last_draw >= interval) {
        sim_mark_dirty();  // Force redraw for time update
        last_draw = now;
    }

    // Refresh display if needed
    sim_update_display();
}

int main(int argc, char **argv) {
    bool fast_mode = false;
    bool batch_mode = false;
    const char *script_file = NULL;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--fast") == 0) {
            fast_mode = true;
        } else if (strcmp(argv[i], "--batch") == 0) {
            batch_mode = true;
        } else if (strcmp(argv[i], "--script") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --script requires a filename\n");
                return 1;
            }
            script_file = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    // Setup signal handlers for clean exit
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Create input source
    if (script_file) {
        input_source = input_source_script_create(script_file);
        if (!input_source) {
            return 1;  // Error already printed
        }
    } else {
        input_source = input_source_keyboard_create();
        if (!input_source) {
            fprintf(stderr, "Error: Failed to create keyboard input\n");
            return 1;
        }
    }

    // Set batch mode before init (affects terminal setup)
    if (batch_mode) {
        sim_set_batch_mode(true);
    }

    // Initialize simulator display
    sim_init();

    // Set realtime mode based on input source (scripts run fast by default)
    if (fast_mode || !input_source->is_realtime(input_source)) {
        sim_set_realtime(false);
    }

    // Point global HAL to simulator HAL
    p_hal = sim_get_hal();

    // Initialize hardware (via sim HAL)
    p_hal->init();

    // Run app initialization
    AppSettings settings;
    AppInitResult init_result = app_init_run(&settings);

    if (init_result == APP_INIT_OK_FACTORY_RESET) {
        sim_log_event("Factory reset performed");
    } else if (init_result == APP_INIT_OK_DEFAULTS) {
        sim_log_event("Using default settings");
    }

    // Initialize coordinator
    Coordinator coordinator;
    coordinator_init(&coordinator, &settings);

    // Restore mode from saved settings
    if (settings.mode < MODE_COUNT) {
        coordinator_set_mode(&coordinator, (ModeState)settings.mode);
    }

    // Start coordinator
    coordinator_start(&coordinator);

    sim_log_event("App initialized, mode=%d", coordinator_get_mode(&coordinator));

    // Main loop
    while (running) {
        // Process input
        if (!input_source->update(input_source, p_hal->millis())) {
            break;
        }

        // Update coordinator (processes inputs, runs mode handlers)
        coordinator_update(&coordinator);

        // Update output pin based on coordinator output state
        if (coordinator_get_output(&coordinator)) {
            p_hal->set_pin(p_hal->sig_out_pin);
        } else {
            p_hal->clear_pin(p_hal->sig_out_pin);
        }

        // Advance time and handle display
        sim_tick();
    }

    // Get result before cleanup
    bool failed = input_source->has_failed(input_source);

    // Cleanup
    input_source->cleanup(input_source);
    sim_cleanup();

    // Return non-zero exit code if any assertions failed
    return failed ? 1 : 0;
}
