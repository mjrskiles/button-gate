#include "output/led_feedback.h"
#include "output/neopixel.h"
#include "output/led_animation.h"
#include "core/states.h"

/**
 * @file led_feedback.c
 * @brief High-level LED feedback controller implementation
 */

// Mode colors (indexed by ModeState)
static const NeopixelColor mode_colors[] = {
    {LED_COLOR_GATE_R,    LED_COLOR_GATE_G,    LED_COLOR_GATE_B},      // MODE_GATE - Green
    {LED_COLOR_TRIGGER_R, LED_COLOR_TRIGGER_G, LED_COLOR_TRIGGER_B},  // MODE_TRIGGER - Cyan
    {LED_COLOR_TOGGLE_R,  LED_COLOR_TOGGLE_G,  LED_COLOR_TOGGLE_B},   // MODE_TOGGLE - Orange
    {LED_COLOR_DIVIDE_R,  LED_COLOR_DIVIDE_G,  LED_COLOR_DIVIDE_B},   // MODE_DIVIDE - Magenta
    {LED_COLOR_CYCLE_R,   LED_COLOR_CYCLE_G,   LED_COLOR_CYCLE_B},    // MODE_CYCLE - Yellow
};

// Page colors (indexed by MenuPage)
// Group by mode association for visual consistency
static const NeopixelColor page_colors[] = {
    {  0, 255,   0},    // PAGE_GATE_CV - Green (gate)
    {  0, 128, 255},    // PAGE_TRIGGER_BEHAVIOR - Cyan (trigger)
    {  0,  64, 192},    // PAGE_TRIGGER_PULSE_LEN - Darker cyan
    {255,  64,   0},    // PAGE_TOGGLE_BEHAVIOR - Orange (toggle)
    {255,   0, 255},    // PAGE_DIVIDE_DIVISOR - Magenta (divide)
    {255, 255,   0},    // PAGE_CYCLE_PATTERN - Yellow (cycle)
    {255, 255, 255},    // PAGE_CV_GLOBAL - White (global)
    {128, 128, 128},    // PAGE_MENU_TIMEOUT - Gray (global)
};

void led_feedback_init(LEDFeedbackController *ctrl) {
    if (!ctrl) return;

    neopixel_init();

    led_animation_init(&ctrl->mode_anim);
    led_animation_init(&ctrl->activity_anim);

    ctrl->in_menu = false;
    ctrl->current_mode = 0;
    ctrl->current_page = 0;

    // Set initial mode color
    led_feedback_set_mode(ctrl, 0);
}

void led_feedback_update(LEDFeedbackController *ctrl,
                         const LEDFeedback *feedback,
                         uint32_t current_time) {
    if (!ctrl) return;

    if (!ctrl->in_menu && feedback) {
        // In perform mode: use feedback from mode handler

        // Mode LED: show mode color (already set by led_feedback_set_mode)
        led_animation_update(&ctrl->mode_anim, LED_MODE, current_time);

        // Activity LED: show output state with brightness from feedback
        NeopixelColor activity_color = {
            feedback->activity_r,
            feedback->activity_g,
            feedback->activity_b
        };

        if (feedback->activity_brightness == 255) {
            // Full on - static color
            led_animation_set_static(&ctrl->activity_anim, activity_color);
        } else if (feedback->activity_brightness == 0) {
            // Off
            led_animation_set_static(&ctrl->activity_anim, (NeopixelColor){0, 0, 0});
        } else {
            // Partial brightness (used by Cycle mode for pulsing)
            NeopixelColor scaled = led_color_scale(activity_color,
                                                   feedback->activity_brightness);
            led_animation_set_static(&ctrl->activity_anim, scaled);
        }

        led_animation_update(&ctrl->activity_anim, LED_ACTIVITY, current_time);
    } else {
        // In menu mode: mode LED blinks, activity LED shows value

        // Update animations
        led_animation_update(&ctrl->mode_anim, LED_MODE, current_time);
        led_animation_update(&ctrl->activity_anim, LED_ACTIVITY, current_time);
    }

    // Flush changes to LEDs
    neopixel_flush();
}

void led_feedback_set_mode(LEDFeedbackController *ctrl, uint8_t mode) {
    if (!ctrl) return;
    if (mode >= MODE_COUNT) mode = 0;

    ctrl->current_mode = mode;

    if (!ctrl->in_menu) {
        // Static mode color in perform mode
        NeopixelColor color = led_feedback_get_mode_color(mode);
        led_animation_set_static(&ctrl->mode_anim, color);
    }
}

void led_feedback_enter_menu(LEDFeedbackController *ctrl, uint8_t page) {
    if (!ctrl) return;

    ctrl->in_menu = true;
    ctrl->current_page = page;

    // Blink the page color to indicate menu mode
    NeopixelColor page_color = led_feedback_get_page_color(page);
    led_animation_set(&ctrl->mode_anim, ANIM_BLINK, page_color, ANIM_BLINK_PERIOD_MS);

    // Activity LED off in menu
    led_animation_stop(&ctrl->activity_anim, LED_ACTIVITY);
}

void led_feedback_exit_menu(LEDFeedbackController *ctrl) {
    if (!ctrl) return;

    ctrl->in_menu = false;

    // Restore static mode color
    NeopixelColor mode_color = led_feedback_get_mode_color(ctrl->current_mode);
    led_animation_set_static(&ctrl->mode_anim, mode_color);
}

void led_feedback_set_page(LEDFeedbackController *ctrl, uint8_t page) {
    if (!ctrl) return;
    if (page >= PAGE_COUNT) page = 0;

    ctrl->current_page = page;

    if (ctrl->in_menu) {
        NeopixelColor page_color = led_feedback_get_page_color(page);
        led_animation_set(&ctrl->mode_anim, ANIM_BLINK, page_color, ANIM_BLINK_PERIOD_MS);
    }
}

void led_feedback_flash(LEDFeedbackController *ctrl,
                        uint8_t r, uint8_t g, uint8_t b) {
    if (!ctrl) return;

    // Quick blink on activity LED
    NeopixelColor color = {r, g, b};
    led_animation_set(&ctrl->activity_anim, ANIM_BLINK, color, 200);
}

NeopixelColor led_feedback_get_mode_color(uint8_t mode) {
    if (mode >= MODE_COUNT) {
        return (NeopixelColor){0, 0, 0};
    }
    return mode_colors[mode];
}

NeopixelColor led_feedback_get_page_color(uint8_t page) {
    if (page >= PAGE_COUNT) {
        return (NeopixelColor){128, 128, 128};  // Gray for unknown
    }
    return page_colors[page];
}
