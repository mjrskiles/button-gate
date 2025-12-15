# FDP-010: Simulator Architecture Refactor

## Status

Proposed

## Summary

Refactor the simulator to reduce coupling between sim-specific code and application logic. The goal is for `sim_main.c` to closely mirror `main.c`, with simulator concerns (input handling, state observation, rendering) cleanly separated.

Additionally, improve keyboard input to support separate press/release actions instead of toggle-only behavior.

## Motivation

Current issues identified in audit:

1. **LED feedback only exists in simulator** - `LEDFeedbackController` is managed in `sim_main.c` but not in `main.c`. Real firmware won't have working Neopixels.

2. **sim_main.c reaches into coordinator internals** - Line 361 accesses `coordinator.cv_input` directly instead of using an API.

3. **Duplicate keyboard handling** - `process_keyboard_input()` in sim_main.c duplicates what InputSource should do.

4. **Toggle-only button input** - Unix terminals don't provide key release events, so buttons toggle instead of acting like gates. This makes testing gesture sequences awkward.

## Detailed Design

### Overview

The refactor has three main parts:

1. **Move LED feedback into the application** (main.c)
2. **Clean up coordinator API** (add cv_state getter)
3. **Refactor simulator input/observation** (sim_main.c simplification)
4. **Improve keyboard input** (separate press/release keys)

### Part 1: LED Feedback in Application

Currently, the LED feedback flow is:
```
sim_main.c → LEDFeedbackController → neopixel API → sim_neopixel.c
```

It should be:
```
main.c → LEDFeedbackController → neopixel API → hal (or sim_neopixel)
sim_main.c just observes via sim_get_led()
```

**Option A: LED feedback in main.c**
```c
// main.c
static LEDFeedbackController led_ctrl;

int main(void) {
    // ...init...
    led_feedback_init(&led_ctrl);

    while (1) {
        coordinator_update(&coordinator);

        // LED feedback
        LEDFeedback feedback;
        coordinator_get_led_feedback(&coordinator, &feedback);
        led_feedback_update(&led_ctrl, &feedback, p_hal->millis());

        // Output
        if (coordinator_get_output(&coordinator)) {
            p_hal->set_pin(p_hal->sig_out_pin);
        } else {
            p_hal->clear_pin(p_hal->sig_out_pin);
        }
    }
}
```

**Option B: LED feedback inside coordinator**

Move `LEDFeedbackController` into `Coordinator` struct and call update internally.

```c
// coordinator_update() internally calls:
led_feedback_update(&coord->led_ctrl, &feedback, p_hal->millis());
neopixel_flush();  // Or let led_feedback_update call it
```

**Recommendation:** Option A - keeps coordinator focused on FSM/event logic, LED is presentation concern.

### Part 2: Coordinator API Cleanup

Add getter for CV input state instead of reaching into internals:

```c
// coordinator.h
bool coordinator_get_cv_state(const Coordinator *coord);

// coordinator.c
bool coordinator_get_cv_state(const Coordinator *coord) {
    return cv_input_get_state(&coord->cv_input);
}
```

### Part 3: Simplified sim_main.c

After Parts 1-2, sim_main.c becomes much simpler:

```c
int main(int argc, char **argv) {
    // Parse args, setup input_source, renderer...

    // === SAME AS main.c ===
    p_hal = sim_get_hal();
    p_hal->init();

    AppSettings settings;
    app_init_run(&settings);

    Coordinator coordinator;
    coordinator_init(&coordinator, &settings);
    coordinator_start(&coordinator);

    LEDFeedbackController led_ctrl;
    led_feedback_init(&led_ctrl);
    // === END SAME ===

    while (running) {
        // Input (sim-specific)
        if (!input_source->update(input_source, p_hal->millis())) break;

        // === SAME AS main.c ===
        coordinator_update(&coordinator);

        LEDFeedback feedback;
        coordinator_get_led_feedback(&coordinator, &feedback);
        led_feedback_update(&led_ctrl, &feedback, p_hal->millis());

        if (coordinator_get_output(&coordinator)) {
            p_hal->set_pin(p_hal->sig_out_pin);
        } else {
            p_hal->clear_pin(p_hal->sig_out_pin);
        }
        // === END SAME ===

        // Observation & rendering (sim-specific)
        observe_state(&sim_state, &coordinator);
        p_hal->advance_time(1);
        renderer->render(renderer, &sim_state);
    }
}
```

The `observe_state()` function would:
- Query coordinator via public API only
- Read LED state via `sim_get_led()`
- Read output via `sim_get_output()` or pin state

### Part 4: Keyboard Input Improvements

**New key mapping:**

| Key | Action |
|-----|--------|
| `a` | Press button A |
| `s` | Release button A |
| `A` | Toggle button A (legacy) |
| `b` | Press button B |
| `n` | Release button B |
| `B` | Toggle button B (legacy) |
| `c` | CV high (5V) |
| `v` | CV low (0V) |
| `C` | Toggle CV (legacy) |
| `+`/`-` | Adjust CV |

**Implementation:**

Move keyboard handling entirely into `input_source_keyboard`:

```c
// input_source.c - keyboard implementation
static bool keyboard_update(InputSource *self, uint32_t time_ms) {
    while (kb_kbhit()) {
        int ch = kb_getch();
        switch (ch) {
            case 'a': sim_set_button_a(true); break;
            case 's': sim_set_button_a(false); break;
            case 'A': sim_set_button_a(!sim_get_button_a()); break;
            // ... etc
            case 'q': case 'Q': case 27: return false;
        }
    }
    return true;
}
```

Remove `process_keyboard_input()` from sim_main.c entirely.

## File Changes

| File | Change | Description |
|------|--------|-------------|
| `src/main.c` | Modify | Add LED feedback controller and update calls |
| `include/core/coordinator.h` | Modify | Add `coordinator_get_cv_state()` |
| `src/core/coordinator.c` | Modify | Implement `coordinator_get_cv_state()` |
| `sim/sim_main.c` | Modify | Remove LED management, remove `process_keyboard_input()`, simplify state tracking |
| `sim/input_source.c` | Modify | Move keyboard handling here, add press/release keys |
| `sim/sim_state.c` | Modify | Add `observe_state()` helper (optional) |

## Implementation Phases

### Phase 1: LED Feedback in Firmware
1. Add LED feedback to `main.c`
2. Verify firmware builds and Neopixels work (needs hardware test)
3. Sim still works (observes same state)

### Phase 2: API Cleanup
1. Add `coordinator_get_cv_state()`
2. Update sim_main.c to use it
3. Remove direct access to `coordinator.cv_input`

### Phase 3: Input Refactor
1. Move keyboard handling to `input_source_keyboard`
2. Add press/release key support
3. Remove `process_keyboard_input()` from sim_main.c
4. Update help text

### Phase 4: Simplify sim_main.c
1. Extract `observe_state()` helper
2. Make main loop mirror main.c structure
3. Remove any remaining coupling

## Alternatives Considered

### SDL for true key events
Would provide actual key up/down events, but adds a heavy dependency for a simple CLI tool. Separate press/release keys achieve the goal without dependencies.

### Timeout-based auto-release
After key press, auto-release after N ms. Feels artificial and timing-dependent. Explicit release is clearer.

### LED feedback inside coordinator
Could encapsulate LED control entirely in coordinator. Rejected because LED is a presentation concern, and coordinator should focus on FSM/event logic.

## Open Questions

1. **Menu tracking for LED animations** - Currently sim_main.c tracks menu enter/exit for LED animations. Should this be automatic in led_feedback based on state queries?

2. **Neopixel flush timing** - Should `neopixel_flush()` be called every loop iteration, or only when dirty? Currently `led_feedback_update()` calls it.

3. **Key repeat handling** - When user holds a key, terminal sends repeated key presses. Should we ignore repeats for press events? (Only first 'a' registers, subsequent 'a's ignored until 's' is pressed)

## References

- [Audit discussion in this session]
- `sim/sim_main.c` - current implementation
- `src/main.c` - firmware main loop


# Michael's notes:

Open Q's

1. I think I need more info to answer this question. I don't want any sim magic to track the program state outside of what the app naturally produces. That said, if we need or already have some kind of signal for events let's consider it. 

2. Only when dirty

3. See my full notes on the keyboard issue.

Notes on keyboard:

I think we need to think through this a bit more. The problem was less 'same key presses and releases' and more about it being cumbersome to have to press twice and manage the mental overhead of remembering the extra step. I think it might be more helpful to just have a second button that wait a fixed duration before sending the button release event. Or if we wanted to get really fancy, macro playback
