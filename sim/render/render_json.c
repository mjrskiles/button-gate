#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file render_json.c
 * @brief JSON renderer for simulator
 *
 * Outputs simulator state as newline-delimited JSON (NDJSON).
 * Each state change produces one JSON object on stdout.
 */

typedef struct {
    bool stream_mode;
    int last_event_count;
} JsonCtx;

/**
 * Escape a string for JSON output.
 * Writes escaped string to buffer, returns bytes written.
 */
static int json_escape_string(char *buf, size_t buf_size, const char *str) {
    size_t i = 0;
    while (*str && i < buf_size - 1) {
        switch (*str) {
            case '"':
                if (i + 2 >= buf_size) goto done;
                buf[i++] = '\\';
                buf[i++] = '"';
                break;
            case '\\':
                if (i + 2 >= buf_size) goto done;
                buf[i++] = '\\';
                buf[i++] = '\\';
                break;
            case '\n':
                if (i + 2 >= buf_size) goto done;
                buf[i++] = '\\';
                buf[i++] = 'n';
                break;
            case '\r':
                if (i + 2 >= buf_size) goto done;
                buf[i++] = '\\';
                buf[i++] = 'r';
                break;
            case '\t':
                if (i + 2 >= buf_size) goto done;
                buf[i++] = '\\';
                buf[i++] = 't';
                break;
            default:
                buf[i++] = *str;
                break;
        }
        str++;
    }
done:
    buf[i] = '\0';
    return (int)i;
}

static void json_init(Renderer *self) {
    (void)self;
    // No terminal setup needed
}

static void json_render(Renderer *self, const SimState *state) {
    JsonCtx *ctx = (JsonCtx*)self->ctx;
    char escaped[256];

    // Output JSON object
    printf("{");

    // Version and timestamp
    printf("\"version\":%d,", state->version);
    printf("\"timestamp_ms\":%lu,", (unsigned long)state->timestamp_ms);

    // State
    printf("\"state\":{");
    printf("\"top\":\"%s\",", sim_top_state_str(state->top_state));
    printf("\"mode\":\"%s\",", sim_mode_str(state->mode));
    if (state->in_menu) {
        printf("\"page\":\"%s\"", sim_page_str(state->page));
    } else {
        printf("\"page\":null");
    }
    printf("},");

    // Inputs
    printf("\"inputs\":{");
    printf("\"button_a\":%s,", state->button_a ? "true" : "false");
    printf("\"button_b\":%s,", state->button_b ? "true" : "false");
    printf("\"cv_in\":%s", state->cv_in ? "true" : "false");
    printf("},");

    // Outputs
    printf("\"outputs\":{");
    printf("\"signal\":%s", state->signal_out ? "true" : "false");
    printf("},");

    // LEDs
    printf("\"leds\":[");
    const char* led_names[] = {"mode", "activity"};
    for (int i = 0; i < SIM_NUM_LEDS; i++) {
        if (i > 0) printf(",");
        printf("{\"index\":%d,\"name\":\"%s\",\"r\":%d,\"g\":%d,\"b\":%d}",
               i, led_names[i],
               state->leds[i].r, state->leds[i].g, state->leds[i].b);
    }
    printf("],");

    // Events (only new events since last render)
    printf("\"events\":[");
    int start = (state->event_count < SIM_MAX_EVENTS) ? 0 : state->event_head;
    int count = (state->event_count < SIM_MAX_EVENTS) ? state->event_count : SIM_MAX_EVENTS;

    // In stream mode, output all events; otherwise just new ones
    int events_to_output = ctx->stream_mode ? count : (state->event_count - ctx->last_event_count);
    if (events_to_output > count) events_to_output = count;
    if (events_to_output < 0) events_to_output = 0;

    int output_start = (start + count - events_to_output) % SIM_MAX_EVENTS;
    bool first = true;
    for (int i = 0; i < events_to_output; i++) {
        int idx = (output_start + i) % SIM_MAX_EVENTS;
        if (!first) printf(",");
        first = false;

        json_escape_string(escaped, sizeof(escaped), state->events[idx].message);
        printf("{\"time_ms\":%lu,\"type\":\"%s\",\"message\":\"%s\"}",
               (unsigned long)state->events[idx].time_ms,
               sim_event_type_str(state->events[idx].type),
               escaped);
    }
    printf("]");

    printf("}\n");
    fflush(stdout);

    ctx->last_event_count = state->event_count;
}

static bool json_handle_input(Renderer *self, SimState *state, int key) {
    (void)self;
    (void)state;
    // JSON mode doesn't handle keyboard input interactively
    // Quit on 'q' or ESC for consistency
    if (key == 'q' || key == 'Q' || key == 27) {
        return false;
    }
    return true;
}

static void json_cleanup(Renderer *self) {
    (void)self;
    // No cleanup needed
}

Renderer* render_json_create(bool stream_mode) {
    Renderer *r = malloc(sizeof(Renderer));
    if (!r) return NULL;

    JsonCtx *ctx = malloc(sizeof(JsonCtx));
    if (!ctx) {
        free(r);
        return NULL;
    }

    ctx->stream_mode = stream_mode;
    ctx->last_event_count = 0;

    r->init = json_init;
    r->render = json_render;
    r->handle_input = json_handle_input;
    r->cleanup = json_cleanup;
    r->ctx = ctx;

    return r;
}
