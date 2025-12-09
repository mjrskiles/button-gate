#include "input_source.h"
#include "sim_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

// Simple logging for script mode (outputs to stdout)
static void script_log(uint32_t time_ms, const char *fmt, ...) {
    printf("[%8lu ms] ", (unsigned long)time_ms);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

// =============================================================================
// Keyboard Input Source
// =============================================================================

typedef struct {
    struct termios orig_termios;
    bool terminal_raw;
    bool realtime;
} KeyboardCtx;

static bool keyboard_update(InputSource *self, uint32_t current_time_ms) {
    (void)current_time_ms;
    (void)self;
    // Keyboard input is now handled directly in sim_main.c for terminal mode.
    // This function is a no-op but keeps the interface consistent.
    return true;
}

static bool keyboard_is_realtime(InputSource *self) {
    KeyboardCtx *ctx = (KeyboardCtx*)self->ctx;
    return ctx->realtime;
}

static bool keyboard_has_failed(InputSource *self) {
    (void)self;
    return false;  // Keyboard never "fails"
}

static void keyboard_cleanup(InputSource *self) {
    KeyboardCtx *ctx = (KeyboardCtx*)self->ctx;
    if (ctx->terminal_raw) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &ctx->orig_termios);
    }
    free(ctx);
    free(self);
}

InputSource* input_source_keyboard_create(void) {
    InputSource *src = malloc(sizeof(InputSource));
    KeyboardCtx *ctx = malloc(sizeof(KeyboardCtx));
    if (!src || !ctx) {
        free(src);
        free(ctx);
        return NULL;
    }

    // Set terminal to raw mode
    tcgetattr(STDIN_FILENO, &ctx->orig_termios);
    struct termios raw = ctx->orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    ctx->terminal_raw = true;
    ctx->realtime = true;

    src->update = keyboard_update;
    src->is_realtime = keyboard_is_realtime;
    src->has_failed = keyboard_has_failed;
    src->cleanup = keyboard_cleanup;
    src->ctx = ctx;

    return src;
}

// =============================================================================
// Script Input Source
// =============================================================================

typedef enum {
    ACT_PRESS,
    ACT_RELEASE,
    ACT_ASSERT,
    ACT_QUIT,
    ACT_LOG
} ScriptAction;

typedef enum {
    TGT_BUTTON_A,
    TGT_BUTTON_B,
    TGT_CV,
    TGT_OUTPUT
} ScriptTarget;

typedef struct {
    uint32_t time_ms;       // Absolute time to execute
    ScriptAction action;
    ScriptTarget target;
    bool value;             // For assert: expected value
    char message[128];      // For log action (generous size on x86)
} ScriptEvent;

typedef struct {
    ScriptEvent *events;
    int event_count;
    int event_capacity;
    int current_event;
    bool failed;
} ScriptCtx;

static bool parse_target(const char *str, ScriptTarget *target) {
    if (strcmp(str, "a") == 0 || strcmp(str, "button_a") == 0) {
        *target = TGT_BUTTON_A;
        return true;
    }
    if (strcmp(str, "b") == 0 || strcmp(str, "button_b") == 0) {
        *target = TGT_BUTTON_B;
        return true;
    }
    if (strcmp(str, "cv") == 0 || strcmp(str, "cv_in") == 0) {
        *target = TGT_CV;
        return true;
    }
    if (strcmp(str, "output") == 0 || strcmp(str, "out") == 0) {
        *target = TGT_OUTPUT;
        return true;
    }
    return false;
}

static bool parse_bool(const char *str, bool *value) {
    if (strcmp(str, "high") == 0 || strcmp(str, "1") == 0 || strcmp(str, "true") == 0) {
        *value = true;
        return true;
    }
    if (strcmp(str, "low") == 0 || strcmp(str, "0") == 0 || strcmp(str, "false") == 0) {
        *value = false;
        return true;
    }
    return false;
}

static void script_add_event(ScriptCtx *ctx, ScriptEvent *evt) {
    if (ctx->event_count >= ctx->event_capacity) {
        ctx->event_capacity = ctx->event_capacity ? ctx->event_capacity * 2 : 32;
        ctx->events = realloc(ctx->events, ctx->event_capacity * sizeof(ScriptEvent));
    }
    ctx->events[ctx->event_count++] = *evt;
}

static bool parse_script(ScriptCtx *ctx, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open script file: %s\n", filename);
        return false;
    }

    char line[256];
    int line_num = 0;
    uint32_t current_time = 0;

    while (fgets(line, sizeof(line), f)) {
        line_num++;

        // Strip comments and trailing whitespace
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';

        char *end = line + strlen(line) - 1;
        while (end > line && isspace(*end)) *end-- = '\0';

        // Skip empty lines
        char *p = line;
        while (*p && isspace(*p)) p++;
        if (*p == '\0') continue;

        // Parse timestamp
        bool absolute_time = false;
        if (*p == '@') {
            absolute_time = true;
            p++;
        }

        char *endptr;
        uint32_t time_val = strtoul(p, &endptr, 10);
        if (endptr == p) {
            fprintf(stderr, "Script error line %d: expected timestamp\n", line_num);
            fclose(f);
            return false;
        }
        p = endptr;

        if (absolute_time) {
            current_time = time_val;
        } else {
            current_time += time_val;
        }

        // Skip whitespace
        while (*p && isspace(*p)) p++;

        // Parse action
        char action_str[32], arg1[32], arg2[64];
        arg1[0] = arg2[0] = '\0';

        int n = sscanf(p, "%31s %31s %63[^\n]", action_str, arg1, arg2);
        if (n < 1) {
            fprintf(stderr, "Script error line %d: expected action\n", line_num);
            fclose(f);
            return false;
        }

        ScriptEvent evt = {0};
        evt.time_ms = current_time;

        // Convert to lowercase
        for (char *s = action_str; *s; s++) *s = tolower(*s);
        for (char *s = arg1; *s; s++) *s = tolower(*s);

        if (strcmp(action_str, "press") == 0) {
            evt.action = ACT_PRESS;
            if (!parse_target(arg1, &evt.target)) {
                fprintf(stderr, "Script error line %d: invalid target '%s'\n", line_num, arg1);
                fclose(f);
                return false;
            }
        } else if (strcmp(action_str, "release") == 0) {
            evt.action = ACT_RELEASE;
            if (!parse_target(arg1, &evt.target)) {
                fprintf(stderr, "Script error line %d: invalid target '%s'\n", line_num, arg1);
                fclose(f);
                return false;
            }
        } else if (strcmp(action_str, "assert") == 0) {
            evt.action = ACT_ASSERT;
            if (!parse_target(arg1, &evt.target)) {
                fprintf(stderr, "Script error line %d: invalid target '%s'\n", line_num, arg1);
                fclose(f);
                return false;
            }
            // For arg2, don't lowercase - parse separately
            char arg2_lower[64];
            strncpy(arg2_lower, arg2, sizeof(arg2_lower) - 1);
            for (char *s = arg2_lower; *s; s++) *s = tolower(*s);
            if (!parse_bool(arg2_lower, &evt.value)) {
                fprintf(stderr, "Script error line %d: invalid value '%s'\n", line_num, arg2);
                fclose(f);
                return false;
            }
        } else if (strcmp(action_str, "log") == 0) {
            evt.action = ACT_LOG;
            // Reconstruct message from arg1 + arg2
            snprintf(evt.message, sizeof(evt.message), "%s %s", arg1, arg2);
        } else if (strcmp(action_str, "quit") == 0 || strcmp(action_str, "exit") == 0) {
            evt.action = ACT_QUIT;
        } else {
            fprintf(stderr, "Script error line %d: unknown action '%s'\n", line_num, action_str);
            fclose(f);
            return false;
        }

        script_add_event(ctx, &evt);
    }

    fclose(f);
    return true;
}

static bool script_update(InputSource *self, uint32_t current_time_ms) {
    ScriptCtx *ctx = (ScriptCtx*)self->ctx;

    // Process all events due at or before current time
    while (ctx->current_event < ctx->event_count) {
        ScriptEvent *evt = &ctx->events[ctx->current_event];

        if (evt->time_ms > current_time_ms) {
            break;  // Not yet time for this event
        }

        // Execute event
        switch (evt->action) {
            case ACT_PRESS:
                switch (evt->target) {
                    case TGT_BUTTON_A:
                        sim_set_button_a(true);
                        script_log(sim_get_time(), "Script: Button A pressed");
                        break;
                    case TGT_BUTTON_B:
                        sim_set_button_b(true);
                        script_log(sim_get_time(), "Script: Button B pressed");
                        break;
                    case TGT_CV:
                        sim_set_cv_voltage(255);  // 5V = HIGH
                        script_log(sim_get_time(), "Script: CV high (5V)");
                        break;
                    default:
                        break;
                }
                break;

            case ACT_RELEASE:
                switch (evt->target) {
                    case TGT_BUTTON_A:
                        sim_set_button_a(false);
                        script_log(sim_get_time(), "Script: Button A released");
                        break;
                    case TGT_BUTTON_B:
                        sim_set_button_b(false);
                        script_log(sim_get_time(), "Script: Button B released");
                        break;
                    case TGT_CV:
                        sim_set_cv_voltage(0);  // 0V = LOW
                        script_log(sim_get_time(), "Script: CV low (0V)");
                        break;
                    default:
                        break;
                }
                break;

            case ACT_ASSERT: {
                bool actual = false;
                const char *name = "";
                switch (evt->target) {
                    case TGT_OUTPUT:
                        actual = sim_get_output();
                        name = "Output";
                        break;
                    case TGT_BUTTON_A:
                        actual = sim_get_button_a();
                        name = "Button A";
                        break;
                    case TGT_BUTTON_B:
                        actual = sim_get_button_b();
                        name = "Button B";
                        break;
                    default:
                        name = "Unknown";
                        break;
                }
                if (actual != evt->value) {
                    script_log(sim_get_time(), "ASSERT FAILED: %s expected %s, got %s",
                                  name,
                                  evt->value ? "HIGH" : "LOW",
                                  actual ? "HIGH" : "LOW");
                    ctx->failed = true;
                } else {
                    script_log(sim_get_time(), "ASSERT OK: %s is %s", name, actual ? "HIGH" : "LOW");
                }
                break;
            }

            case ACT_LOG:
                script_log(sim_get_time(), "Script: %s", evt->message);
                break;

            case ACT_QUIT:
                script_log(sim_get_time(), "Script: quit");
                return false;
        }

        ctx->current_event++;
    }

    // If we've processed all events and there's no quit, auto-quit
    if (ctx->current_event >= ctx->event_count) {
        script_log(sim_get_time(), "Script: end of script");
        return false;
    }

    return true;
}

static bool script_is_realtime(InputSource *self) {
    (void)self;
    return false;  // Scripts always run fast
}

static bool script_has_failed(InputSource *self) {
    ScriptCtx *ctx = (ScriptCtx*)self->ctx;
    return ctx->failed;
}

static void script_cleanup(InputSource *self) {
    ScriptCtx *ctx = (ScriptCtx*)self->ctx;
    if (ctx->failed) {
        fprintf(stderr, "\nScript completed with FAILURES\n");
    } else {
        fprintf(stderr, "\nScript completed successfully\n");
    }
    free(ctx->events);
    free(ctx);
    free(self);
}

InputSource* input_source_script_create(const char *filename) {
    InputSource *src = malloc(sizeof(InputSource));
    ScriptCtx *ctx = calloc(1, sizeof(ScriptCtx));
    if (!src || !ctx) {
        free(src);
        free(ctx);
        return NULL;
    }

    if (!parse_script(ctx, filename)) {
        free(ctx);
        free(src);
        return NULL;
    }

    src->update = script_update;
    src->is_realtime = script_is_realtime;
    src->has_failed = script_has_failed;
    src->cleanup = script_cleanup;
    src->ctx = ctx;

    return src;
}
