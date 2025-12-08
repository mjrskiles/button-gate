#include "fsm/fsm.h"
#include <stddef.h>

/**
 * Helper: Find state by ID in state array
 */
static const State* find_state(const FSM *fsm, uint8_t state_id) {
    if (!fsm || !fsm->states) return NULL;

    for (uint8_t i = 0; i < fsm->num_states; i++) {
        if (fsm->states[i].id == state_id) {
            return &fsm->states[i];
        }
    }
    return NULL;
}

/**
 * Helper: Call entry action for a state (if defined)
 */
static void call_entry_action(const FSM *fsm, uint8_t state_id) {
    const State *state = find_state(fsm, state_id);
    if (state && state->on_enter) {
        state->on_enter();
    }
}

/**
 * Helper: Call exit action for a state (if defined)
 */
static void call_exit_action(const FSM *fsm, uint8_t state_id) {
    const State *state = find_state(fsm, state_id);
    if (state && state->on_exit) {
        state->on_exit();
    }
}

void fsm_init(FSM *fsm, const State *states, uint8_t num_states,
              const Transition *transitions, uint8_t num_trans,
              uint8_t initial) {
    if (!fsm) return;

    fsm->states = states;
    fsm->transitions = transitions;
    fsm->num_states = num_states;
    fsm->num_transitions = num_trans;
    fsm->current_state = initial;
    fsm->initial_state = initial;
    fsm->active = false;
}

void fsm_start(FSM *fsm) {
    if (!fsm) return;

    fsm->active = true;
    call_entry_action(fsm, fsm->current_state);
}

bool fsm_process_event(FSM *fsm, uint8_t event) {
    if (!fsm || !fsm->active || !fsm->transitions) return false;

    // Search transition table for matching (current_state, event)
    for (uint8_t i = 0; i < fsm->num_transitions; i++) {
        const Transition *trans = &fsm->transitions[i];

        // Check if transition matches
        bool state_matches = (trans->from_state == fsm->current_state) ||
                            (trans->from_state == FSM_ANY_STATE);
        bool event_matches = (trans->event == event);

        if (state_matches && event_matches) {
            // Handle FSM_NO_TRANSITION (action only, no state change)
            if (trans->to_state == FSM_NO_TRANSITION) {
                if (trans->action) {
                    trans->action();
                }
                return false;  // No state change
            }

            // Execute full transition
            // 1. Exit current state
            call_exit_action(fsm, fsm->current_state);

            // 2. Execute transition action
            if (trans->action) {
                trans->action();
            }

            // 3. Update state
            fsm->current_state = trans->to_state;

            // 4. Enter new state
            call_entry_action(fsm, fsm->current_state);

            return true;  // State changed
        }
    }

    return false;  // No matching transition
}

void fsm_update(FSM *fsm) {
    if (!fsm || !fsm->active) return;

    const State *state = find_state(fsm, fsm->current_state);
    if (state && state->on_update) {
        state->on_update();
    }
}

void fsm_reset(FSM *fsm) {
    if (!fsm) return;

    // Exit current state
    if (fsm->active) {
        call_exit_action(fsm, fsm->current_state);
    }

    // Reset to initial state
    fsm->current_state = fsm->initial_state;

    // Enter initial state
    if (fsm->active) {
        call_entry_action(fsm, fsm->initial_state);
    }
}

void fsm_stop(FSM *fsm) {
    if (!fsm || !fsm->active) return;

    call_exit_action(fsm, fsm->current_state);
    fsm->active = false;
}

uint8_t fsm_get_state(const FSM *fsm) {
    if (!fsm) return 0;
    return fsm->current_state;
}

void fsm_set_state(FSM *fsm, uint8_t state_id) {
    if (!fsm) return;

    // Exit current state
    if (fsm->active) {
        call_exit_action(fsm, fsm->current_state);
    }

    // Update state
    fsm->current_state = state_id;

    // Enter new state
    if (fsm->active) {
        call_entry_action(fsm, state_id);
    }
}

bool fsm_is_active(const FSM *fsm) {
    if (!fsm) return false;
    return fsm->active;
}
