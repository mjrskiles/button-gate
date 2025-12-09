#include "render.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * @file render_batch.c
 * @brief Plain text batch renderer for simulator
 *
 * Outputs events as simple text lines for CI/scripts.
 * Compatible with existing --batch mode behavior.
 */

typedef struct {
    int last_event_count;
} BatchCtx;

static void batch_init(Renderer *self) {
    (void)self;
    // No terminal setup needed
}

static void batch_render(Renderer *self, const SimState *state) {
    BatchCtx *ctx = (BatchCtx*)self->ctx;

    // Only output new events
    int start = (state->event_count < SIM_MAX_EVENTS) ? 0 : state->event_head;
    int count = (state->event_count < SIM_MAX_EVENTS) ? state->event_count : SIM_MAX_EVENTS;

    int new_events = state->event_count - ctx->last_event_count;
    if (new_events > count) new_events = count;
    if (new_events < 0) new_events = 0;

    int output_start = (start + count - new_events) % SIM_MAX_EVENTS;

    for (int i = 0; i < new_events; i++) {
        int idx = (output_start + i) % SIM_MAX_EVENTS;
        printf("[%8lu ms] %s\n",
               (unsigned long)state->events[idx].time_ms,
               state->events[idx].message);
    }

    if (new_events > 0) {
        fflush(stdout);
    }

    ctx->last_event_count = state->event_count;
}

static bool batch_handle_input(Renderer *self, SimState *state, int key) {
    (void)self;
    (void)state;
    // Batch mode doesn't handle keyboard input
    if (key == 'q' || key == 'Q' || key == 27) {
        return false;
    }
    return true;
}

static void batch_cleanup(Renderer *self) {
    (void)self;
    printf("Simulator exited.\n");
}

Renderer* render_batch_create(void) {
    Renderer *r = malloc(sizeof(Renderer));
    if (!r) return NULL;

    BatchCtx *ctx = malloc(sizeof(BatchCtx));
    if (!ctx) {
        free(r);
        return NULL;
    }

    ctx->last_event_count = 0;

    r->init = batch_init;
    r->render = batch_render;
    r->handle_input = batch_handle_input;
    r->cleanup = batch_cleanup;
    r->ctx = ctx;

    return r;
}
