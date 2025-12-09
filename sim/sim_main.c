#include "sim_hal.h"
#include "sim_state.h"
#include "input_source.h"
#include "render/render.h"
#include "hardware/hal_interface.h"
#include "app_init.h"
#include "core/coordinator.h"
#include "input/cv_input.h"
#include "output/led_feedback.h"
#include "output/neopixel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>

/**
 * @file sim_main.c
 * @brief Gatekeeper x86 simulator entry point
 *
 * Headless architecture: state is collected into SimState,
 * then rendered by the selected renderer (terminal, JSON, batch).
 */

static volatile bool running = true;
static InputSource *input_source = NULL;
static Renderer *renderer = NULL;
static SimState sim_state;
static LEDFeedbackController led_ctrl;

static void handle_signal(int sig) {
    (void)sig;
    running = false;
}

static int kb_kbhit(void) {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

static int kb_getch(void) {
    unsigned char ch;
    if (read(STDIN_FILENO, &ch, 1) == 1) return ch;
    return -1;
}

// CV voltage step size for +/- keys (in ADC units, ~0.2V per step)
#define CV_VOLTAGE_STEP 10

static void print_usage(const char *progname) {
    printf("Gatekeeper x86 Simulator\n\n");
    printf("Usage: %s [options]\n\n", progname);
    printf("Options:\n");
    printf("  --script <file>  Run script file instead of interactive mode\n");
    printf("  --batch          Batch mode: plain text output (for CI/scripts)\n");
    printf("  --json           JSON output: one object per state change\n");
    printf("  --json-stream    JSON stream: continuous output at fixed interval\n");
    printf("  --fast           Run in fast-forward mode (interactive only)\n");
    printf("  --help           Show this help message\n");
    printf("\n");
    printf("Interactive Controls:\n");
    printf("  A          Toggle Button A\n");
    printf("  B          Toggle Button B\n");
    printf("  C          Toggle CV input (0V <-> 5V)\n");
    printf("  +/-        Adjust CV voltage (+/- 0.2V)\n");
    printf("  R          Reset time\n");
    printf("  F          Toggle fast/realtime mode\n");
    printf("  L          Toggle legend\n");
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
}

// Process keyboard input for interactive mode
static bool process_keyboard_input(void) {
    while (kb_kbhit()) {
        int ch = kb_getch();
        switch (ch) {
            case 'a': case 'A':
                sim_set_button_a(!sim_get_button_a());
                sim_state_add_event(&sim_state, EVT_TYPE_INPUT, sim_get_time(),
                    "Button A %s", sim_get_button_a() ? "pressed" : "released");
                break;

            case 'b': case 'B':
                sim_set_button_b(!sim_get_button_b());
                sim_state_add_event(&sim_state, EVT_TYPE_INPUT, sim_get_time(),
                    "Button B %s", sim_get_button_b() ? "pressed" : "released");
                break;

            case 'c': case 'C': {
                // Toggle CV between 0V and 5V
                uint8_t current = sim_get_cv_voltage();
                uint8_t new_voltage = (current < 128) ? 255 : 0;
                sim_set_cv_voltage(new_voltage);
                uint16_t mv = cv_adc_to_millivolts(new_voltage);
                sim_state_add_event(&sim_state, EVT_TYPE_INPUT, sim_get_time(),
                    "CV -> %u.%uV", mv / 1000, (mv % 1000) / 100);
                break;
            }

            case '+': case '=': {
                // Increase CV voltage
                sim_adjust_cv_voltage(CV_VOLTAGE_STEP);
                uint8_t voltage = sim_get_cv_voltage();
                uint16_t mv = cv_adc_to_millivolts(voltage);
                sim_state_add_event(&sim_state, EVT_TYPE_INPUT, sim_get_time(),
                    "CV -> %u.%uV", mv / 1000, (mv % 1000) / 100);
                break;
            }

            case '-': case '_': {
                // Decrease CV voltage
                sim_adjust_cv_voltage(-CV_VOLTAGE_STEP);
                uint8_t voltage = sim_get_cv_voltage();
                uint16_t mv = cv_adc_to_millivolts(voltage);
                sim_state_add_event(&sim_state, EVT_TYPE_INPUT, sim_get_time(),
                    "CV -> %u.%uV", mv / 1000, (mv % 1000) / 100);
                break;
            }

            case 'r': case 'R':
                p_hal->reset_time();
                sim_state_add_event(&sim_state, EVT_TYPE_INFO, 0, "Time reset");
                break;

            case 'f': case 'F':
                sim_state.realtime_mode = !sim_state.realtime_mode;
                sim_state_add_event(&sim_state, EVT_TYPE_INFO, sim_get_time(),
                    "Speed: %s", sim_state.realtime_mode ? "realtime" : "fast-forward");
                sim_state_mark_dirty(&sim_state);
                break;

            case 'l': case 'L':
                sim_state_toggle_legend(&sim_state);
                break;

            case 'q': case 'Q': case 27:  // ESC
                return false;
        }
    }
    return true;
}

// Update state tracking for state changes
static void track_state_changes(Coordinator *coord) {
    static TopState last_top_state = TOP_PERFORM;
    static ModeState last_mode = MODE_GATE;
    static MenuPage last_page = PAGE_GATE_CV;
    static bool last_output = false;

    TopState top_state = coordinator_get_top_state(coord);
    ModeState mode = coordinator_get_mode(coord);
    MenuPage page = coordinator_get_page(coord);
    bool in_menu = coordinator_in_menu(coord);
    bool output = coordinator_get_output(coord);

    // Log state changes
    if (top_state != last_top_state) {
        sim_state_add_event(&sim_state, EVT_TYPE_STATE_CHANGE, sim_get_time(),
            "State -> %s", sim_top_state_str(top_state));
        last_top_state = top_state;
    }

    if (mode != last_mode) {
        sim_state_add_event(&sim_state, EVT_TYPE_MODE_CHANGE, sim_get_time(),
            "Mode -> %s", sim_mode_str(mode));
        led_feedback_set_mode(&led_ctrl, mode);
        last_mode = mode;
    }

    if (in_menu && page != last_page) {
        sim_state_add_event(&sim_state, EVT_TYPE_PAGE_CHANGE, sim_get_time(),
            "Page -> %s", sim_page_str(page));
        led_feedback_set_page(&led_ctrl, page);
        last_page = page;
    }

    if (output != last_output) {
        sim_state_add_event(&sim_state, EVT_TYPE_OUTPUT, sim_get_time(),
            "Output -> %s", output ? "HIGH" : "LOW");
        last_output = output;
    }

    // Update state struct
    sim_state_set_fsm(&sim_state, top_state, mode, page, in_menu);
    sim_state_set_output(&sim_state, output);
}

int main(int argc, char **argv) {
    bool fast_mode = false;
    bool batch_mode = false;
    bool json_mode = false;
    bool json_stream = false;
    const char *script_file = NULL;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--fast") == 0) {
            fast_mode = true;
        } else if (strcmp(argv[i], "--batch") == 0) {
            batch_mode = true;
        } else if (strcmp(argv[i], "--json") == 0) {
            json_mode = true;
        } else if (strcmp(argv[i], "--json-stream") == 0) {
            json_mode = true;
            json_stream = true;
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

    // Create renderer based on mode
    if (json_mode) {
        renderer = render_json_create(json_stream);
    } else if (batch_mode) {
        renderer = render_batch_create();
    } else {
        renderer = render_terminal_create();
    }

    if (!renderer) {
        fprintf(stderr, "Error: Failed to create renderer\n");
        input_source->cleanup(input_source);
        return 1;
    }

    // Initialize state
    sim_state_init(&sim_state);

    // Set initial mode based on input source
    bool realtime = !fast_mode && input_source->is_realtime(input_source);
    sim_state_set_realtime(&sim_state, realtime);

    // Initialize renderer
    renderer->init(renderer);

    // Point global HAL to simulator HAL
    p_hal = sim_get_hal();

    // Initialize hardware (via sim HAL)
    p_hal->init();

    // Run app initialization
    AppSettings settings;
    AppInitResult init_result = app_init_run(&settings);

    if (init_result == APP_INIT_OK_FACTORY_RESET) {
        sim_state_add_event(&sim_state, EVT_TYPE_INFO, sim_get_time(), "Factory reset performed");
    } else if (init_result == APP_INIT_OK_DEFAULTS) {
        sim_state_add_event(&sim_state, EVT_TYPE_INFO, sim_get_time(), "Using default settings");
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

    // Initialize LED feedback controller
    led_feedback_init(&led_ctrl);
    led_feedback_set_mode(&led_ctrl, coordinator_get_mode(&coordinator));

    sim_state_add_event(&sim_state, EVT_TYPE_INFO, sim_get_time(),
        "App initialized, mode=%s", sim_mode_str(coordinator_get_mode(&coordinator)));

    // Main loop
    uint32_t last_render = 0;
    while (running) {
        // Process input from keyboard (for interactive terminal mode)
        if (!batch_mode && !json_mode && !script_file) {
            if (!process_keyboard_input()) {
                break;
            }
        }

        // Process input from input source (scripts or keyboard adapter)
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

        // Track state changes and update sim_state
        track_state_changes(&coordinator);

        // Update LED feedback from mode handler
        LEDFeedback feedback;
        coordinator_get_led_feedback(&coordinator, &feedback);

        // Track menu state for LED animations
        static bool was_in_menu = false;
        bool in_menu = coordinator_in_menu(&coordinator);
        if (in_menu && !was_in_menu) {
            led_feedback_enter_menu(&led_ctrl, coordinator_get_page(&coordinator));
        } else if (!in_menu && was_in_menu) {
            led_feedback_exit_menu(&led_ctrl);
        }
        was_in_menu = in_menu;

        // Update LED animations
        led_feedback_update(&led_ctrl, &feedback, p_hal->millis());

        // Update input/output state in sim_state
        // Get CV digital state from coordinator's cv_input (after hysteresis)
        bool cv_digital = cv_input_get_state(&coordinator.cv_input);
        sim_state_set_inputs(&sim_state,
            sim_get_button_a(),
            sim_get_button_b(),
            cv_digital,
            sim_get_cv_voltage());

        // Update LED state in sim_state
        for (int i = 0; i < SIM_NUM_LEDS; i++) {
            uint8_t r, g, b;
            sim_get_led(i, &r, &g, &b);
            sim_state_set_led(&sim_state, i, r, g, b);
        }

        // Update timestamp
        sim_state_set_time(&sim_state, p_hal->millis());

        // Advance time
        p_hal->advance_time(1);

        // Real-time pacing if needed
        if (sim_state.realtime_mode && input_source->is_realtime(input_source)) {
            usleep(1000);  // 1ms
        }

        // Render periodically or on state change
        uint32_t now = p_hal->millis();
        uint32_t render_interval = sim_state.realtime_mode ? 100 : 500;

        if (sim_state_is_dirty(&sim_state) || (now - last_render >= render_interval)) {
            renderer->render(renderer, &sim_state);
            sim_state_clear_dirty(&sim_state);
            last_render = now;
        }
    }

    // Get result before cleanup
    bool failed = input_source->has_failed(input_source);

    // Cleanup
    renderer->cleanup(renderer);
    render_destroy(renderer);
    input_source->cleanup(input_source);

    // Return non-zero exit code if any assertions failed
    return failed ? 1 : 0;
}
