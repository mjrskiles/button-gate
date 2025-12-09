#ifndef SIM_RENDER_H
#define SIM_RENDER_H

#include "../sim_state.h"
#include <stdbool.h>

/**
 * @file render.h
 * @brief Renderer interface for simulator output
 *
 * Defines interface for rendering simulator state.
 * Implementations: terminal (ANSI), JSON, batch (plain text).
 */

typedef struct Renderer Renderer;

/**
 * Renderer interface
 */
struct Renderer {
    /**
     * Initialize renderer (e.g., set up terminal mode).
     */
    void (*init)(Renderer *self);

    /**
     * Render current state.
     */
    void (*render)(Renderer *self, const SimState *state);

    /**
     * Handle keyboard input (for interactive renderers).
     * Returns false to quit, true to continue.
     */
    bool (*handle_input)(Renderer *self, SimState *state, int key);

    /**
     * Cleanup renderer (e.g., restore terminal).
     */
    void (*cleanup)(Renderer *self);

    /**
     * Private context for implementation.
     */
    void *ctx;
};

/**
 * Create terminal renderer (ANSI UI).
 */
Renderer* render_terminal_create(void);

/**
 * Create JSON renderer.
 *
 * @param stream_mode If true, output at fixed intervals; if false, only on state change
 */
Renderer* render_json_create(bool stream_mode);

/**
 * Create batch renderer (plain text events only).
 */
Renderer* render_batch_create(void);

/**
 * Destroy renderer and free resources.
 */
void render_destroy(Renderer *renderer);

#endif /* SIM_RENDER_H */
