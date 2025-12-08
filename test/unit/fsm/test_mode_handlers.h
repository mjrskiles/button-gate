#ifndef TEST_MODE_HANDLERS_H
#define TEST_MODE_HANDLERS_H

#include "unity.h"
#include "unity_fixture.h"
#include "modes/mode_handlers.h"
#include "core/states.h"
#include "../mocks/mock_hal.h"

/**
 * @file test_mode_handlers.h
 * @brief Unit tests for mode handlers
 *
 * Tests each mode handler for correct signal processing behavior.
 */

TEST_GROUP(ModeHandlersTests);

TEST_SETUP(ModeHandlersTests) {
    mock_hal_init();
    use_mock_hal();
}

TEST_TEAR_DOWN(ModeHandlersTests) {
    // Nothing to clean up
}

// =============================================================================
// Gate Mode Tests
// =============================================================================

TEST(ModeHandlersTests, TestGateInit) {
    ModeContext ctx;
    mode_handler_init(MODE_GATE, &ctx);
    TEST_ASSERT_FALSE(ctx.gate.output_state);
}

TEST(ModeHandlersTests, TestGateFollowsInput) {
    ModeContext ctx;
    bool output;

    mode_handler_init(MODE_GATE, &ctx);

    // Input LOW -> Output LOW
    mode_handler_process(MODE_GATE, &ctx, false, &output);
    TEST_ASSERT_FALSE(output);

    // Input HIGH -> Output HIGH
    mode_handler_process(MODE_GATE, &ctx, true, &output);
    TEST_ASSERT_TRUE(output);

    // Input LOW -> Output LOW
    mode_handler_process(MODE_GATE, &ctx, false, &output);
    TEST_ASSERT_FALSE(output);
}

TEST(ModeHandlersTests, TestGateReturnsChangedFlag) {
    ModeContext ctx;
    bool output;
    bool changed;

    mode_handler_init(MODE_GATE, &ctx);

    // First call with LOW - no change (already LOW)
    changed = mode_handler_process(MODE_GATE, &ctx, false, &output);
    TEST_ASSERT_FALSE(changed);

    // Change to HIGH - should return true
    changed = mode_handler_process(MODE_GATE, &ctx, true, &output);
    TEST_ASSERT_TRUE(changed);

    // Stay HIGH - no change
    changed = mode_handler_process(MODE_GATE, &ctx, true, &output);
    TEST_ASSERT_FALSE(changed);
}

// =============================================================================
// Trigger Mode Tests
// =============================================================================

TEST(ModeHandlersTests, TestTriggerInit) {
    ModeContext ctx;
    mode_handler_init(MODE_TRIGGER, &ctx);
    TEST_ASSERT_FALSE(ctx.trigger.output_state);
    TEST_ASSERT_FALSE(ctx.trigger.last_input);
    TEST_ASSERT_EQUAL(TRIGGER_PULSE_DEFAULT, ctx.trigger.pulse_duration_ms);
}

TEST(ModeHandlersTests, TestTriggerPulseOnRisingEdge) {
    ModeContext ctx;
    bool output;

    mode_handler_init(MODE_TRIGGER, &ctx);

    // Start with input LOW
    mode_handler_process(MODE_TRIGGER, &ctx, false, &output);
    TEST_ASSERT_FALSE(output);

    // Rising edge -> pulse starts
    mode_handler_process(MODE_TRIGGER, &ctx, true, &output);
    TEST_ASSERT_TRUE(output);
}

TEST(ModeHandlersTests, TestTriggerPulseExpires) {
    ModeContext ctx;
    bool output;

    mode_handler_init(MODE_TRIGGER, &ctx);
    ctx.trigger.pulse_duration_ms = 10;  // 10ms pulse

    // Trigger pulse
    mode_handler_process(MODE_TRIGGER, &ctx, false, &output);
    mode_handler_process(MODE_TRIGGER, &ctx, true, &output);
    TEST_ASSERT_TRUE(output);

    // Advance time but not enough for pulse to expire
    p_hal->advance_time(5);
    mode_handler_process(MODE_TRIGGER, &ctx, true, &output);
    TEST_ASSERT_TRUE(output);

    // Advance time past pulse duration
    p_hal->advance_time(10);
    mode_handler_process(MODE_TRIGGER, &ctx, true, &output);
    TEST_ASSERT_FALSE(output);
}

TEST(ModeHandlersTests, TestTriggerNoRetriggerDuringPulse) {
    ModeContext ctx;
    bool output;

    mode_handler_init(MODE_TRIGGER, &ctx);
    ctx.trigger.pulse_duration_ms = 50;

    // First trigger
    mode_handler_process(MODE_TRIGGER, &ctx, false, &output);
    mode_handler_process(MODE_TRIGGER, &ctx, true, &output);
    TEST_ASSERT_TRUE(output);

    p_hal->advance_time(10);

    // Release and press again (should restart pulse from current time)
    mode_handler_process(MODE_TRIGGER, &ctx, false, &output);
    mode_handler_process(MODE_TRIGGER, &ctx, true, &output);
    TEST_ASSERT_TRUE(output);  // Still in pulse (restarted)

    // Wait for new pulse to expire
    p_hal->advance_time(60);
    mode_handler_process(MODE_TRIGGER, &ctx, true, &output);
    TEST_ASSERT_FALSE(output);
}

// =============================================================================
// Toggle Mode Tests
// =============================================================================

TEST(ModeHandlersTests, TestToggleInit) {
    ModeContext ctx;
    mode_handler_init(MODE_TOGGLE, &ctx);
    TEST_ASSERT_FALSE(ctx.toggle.output_state);
    TEST_ASSERT_FALSE(ctx.toggle.last_input);
}

TEST(ModeHandlersTests, TestToggleFlipsOnRisingEdge) {
    ModeContext ctx;
    bool output;

    mode_handler_init(MODE_TOGGLE, &ctx);

    // Initial state: LOW
    mode_handler_process(MODE_TOGGLE, &ctx, false, &output);
    TEST_ASSERT_FALSE(output);

    // First press -> HIGH
    mode_handler_process(MODE_TOGGLE, &ctx, true, &output);
    TEST_ASSERT_TRUE(output);

    // Release (no change)
    mode_handler_process(MODE_TOGGLE, &ctx, false, &output);
    TEST_ASSERT_TRUE(output);

    // Second press -> LOW
    mode_handler_process(MODE_TOGGLE, &ctx, true, &output);
    TEST_ASSERT_FALSE(output);

    // Release (no change)
    mode_handler_process(MODE_TOGGLE, &ctx, false, &output);
    TEST_ASSERT_FALSE(output);

    // Third press -> HIGH
    mode_handler_process(MODE_TOGGLE, &ctx, true, &output);
    TEST_ASSERT_TRUE(output);
}

TEST(ModeHandlersTests, TestToggleIgnoresHold) {
    ModeContext ctx;
    bool output;

    mode_handler_init(MODE_TOGGLE, &ctx);

    // Press
    mode_handler_process(MODE_TOGGLE, &ctx, false, &output);
    mode_handler_process(MODE_TOGGLE, &ctx, true, &output);
    TEST_ASSERT_TRUE(output);

    // Hold (multiple updates with input HIGH)
    mode_handler_process(MODE_TOGGLE, &ctx, true, &output);
    mode_handler_process(MODE_TOGGLE, &ctx, true, &output);
    mode_handler_process(MODE_TOGGLE, &ctx, true, &output);
    TEST_ASSERT_TRUE(output);  // Still HIGH, didn't toggle multiple times
}

// =============================================================================
// Divide Mode Tests
// =============================================================================

TEST(ModeHandlersTests, TestDivideInit) {
    ModeContext ctx;
    mode_handler_init(MODE_DIVIDE, &ctx);
    TEST_ASSERT_FALSE(ctx.divide.output_state);
    TEST_ASSERT_EQUAL(0, ctx.divide.counter);
    TEST_ASSERT_EQUAL(DIVIDE_DEFAULT, ctx.divide.divisor);
}

TEST(ModeHandlersTests, TestDivideByTwo) {
    ModeContext ctx;
    bool output;

    mode_handler_init(MODE_DIVIDE, &ctx);
    ctx.divide.divisor = 2;

    // First pulse - no output yet
    mode_handler_process(MODE_DIVIDE, &ctx, false, &output);
    mode_handler_process(MODE_DIVIDE, &ctx, true, &output);
    TEST_ASSERT_FALSE(output);
    TEST_ASSERT_EQUAL(1, ctx.divide.counter);

    mode_handler_process(MODE_DIVIDE, &ctx, false, &output);

    // Second pulse - output!
    mode_handler_process(MODE_DIVIDE, &ctx, true, &output);
    TEST_ASSERT_TRUE(output);
    TEST_ASSERT_EQUAL(0, ctx.divide.counter);  // Counter reset

    // Wait for pulse to expire
    p_hal->advance_time(OUTPUT_PULSE_MS + 5);
    mode_handler_process(MODE_DIVIDE, &ctx, false, &output);
    TEST_ASSERT_FALSE(output);
}

TEST(ModeHandlersTests, TestDivideByFour) {
    ModeContext ctx;
    bool output;

    mode_handler_init(MODE_DIVIDE, &ctx);
    ctx.divide.divisor = 4;

    // Pulses 1-3: no output
    for (int i = 0; i < 3; i++) {
        mode_handler_process(MODE_DIVIDE, &ctx, false, &output);
        mode_handler_process(MODE_DIVIDE, &ctx, true, &output);
        TEST_ASSERT_FALSE(output);
        mode_handler_process(MODE_DIVIDE, &ctx, false, &output);
    }

    // Pulse 4: output!
    mode_handler_process(MODE_DIVIDE, &ctx, true, &output);
    TEST_ASSERT_TRUE(output);
}

// =============================================================================
// Cycle Mode Tests
// =============================================================================

TEST(ModeHandlersTests, TestCycleInit) {
    ModeContext ctx;
    mode_handler_init(MODE_CYCLE, &ctx);
    TEST_ASSERT_FALSE(ctx.cycle.output_state);
    TEST_ASSERT_TRUE(ctx.cycle.running);
    TEST_ASSERT_EQUAL(CYCLE_DEFAULT_PERIOD_MS, ctx.cycle.period_ms);
}

TEST(ModeHandlersTests, TestCycleOscillates) {
    ModeContext ctx;
    bool output;

    mode_handler_init(MODE_CYCLE, &ctx);
    ctx.cycle.period_ms = 100;  // 100ms period = 50ms half-period

    // Initial state
    mode_handler_process(MODE_CYCLE, &ctx, false, &output);
    bool initial_state = output;

    // After half-period, should toggle
    p_hal->advance_time(55);
    mode_handler_process(MODE_CYCLE, &ctx, false, &output);
    TEST_ASSERT_NOT_EQUAL(initial_state, output);

    // After another half-period, should toggle back
    p_hal->advance_time(55);
    mode_handler_process(MODE_CYCLE, &ctx, false, &output);
    TEST_ASSERT_EQUAL(initial_state, output);
}

TEST(ModeHandlersTests, TestCycleIgnoresInput) {
    ModeContext ctx;
    bool output1, output2;

    mode_handler_init(MODE_CYCLE, &ctx);
    ctx.cycle.period_ms = 100;

    // Process with input LOW
    mode_handler_process(MODE_CYCLE, &ctx, false, &output1);

    // Process with input HIGH - should produce same output
    mode_handler_process(MODE_CYCLE, &ctx, true, &output2);

    TEST_ASSERT_EQUAL(output1, output2);
}

TEST(ModeHandlersTests, TestCycleDefaultBPM) {
    // 80 BPM = 750ms period
    TEST_ASSERT_EQUAL(750, CYCLE_DEFAULT_PERIOD_MS);
}

// =============================================================================
// LED Feedback Tests
// =============================================================================

TEST(ModeHandlersTests, TestGateLEDColors) {
    ModeContext ctx;
    LEDFeedback fb;

    mode_handler_init(MODE_GATE, &ctx);
    mode_handler_get_led(MODE_GATE, &ctx, &fb);

    // Mode LED should be green
    TEST_ASSERT_EQUAL(LED_COLOR_GATE_R, fb.mode_r);
    TEST_ASSERT_EQUAL(LED_COLOR_GATE_G, fb.mode_g);
    TEST_ASSERT_EQUAL(LED_COLOR_GATE_B, fb.mode_b);
}

TEST(ModeHandlersTests, TestTriggerLEDColors) {
    ModeContext ctx;
    LEDFeedback fb;

    mode_handler_init(MODE_TRIGGER, &ctx);
    mode_handler_get_led(MODE_TRIGGER, &ctx, &fb);

    // Mode LED should be cyan
    TEST_ASSERT_EQUAL(LED_COLOR_TRIGGER_R, fb.mode_r);
    TEST_ASSERT_EQUAL(LED_COLOR_TRIGGER_G, fb.mode_g);
    TEST_ASSERT_EQUAL(LED_COLOR_TRIGGER_B, fb.mode_b);
}

TEST(ModeHandlersTests, TestLEDActivityReflectsOutput) {
    ModeContext ctx;
    LEDFeedback fb;
    bool output;

    mode_handler_init(MODE_GATE, &ctx);

    // Output LOW -> activity brightness 0
    mode_handler_process(MODE_GATE, &ctx, false, &output);
    mode_handler_get_led(MODE_GATE, &ctx, &fb);
    TEST_ASSERT_EQUAL(0, fb.activity_brightness);

    // Output HIGH -> activity brightness 255
    mode_handler_process(MODE_GATE, &ctx, true, &output);
    mode_handler_get_led(MODE_GATE, &ctx, &fb);
    TEST_ASSERT_EQUAL(255, fb.activity_brightness);
}

// =============================================================================
// Null Safety Tests
// =============================================================================

TEST(ModeHandlersTests, TestNullSafety) {
    ModeContext ctx;
    bool output;
    LEDFeedback fb;

    // Should not crash
    mode_handler_init(MODE_GATE, NULL);
    mode_handler_process(MODE_GATE, NULL, false, &output);
    mode_handler_process(MODE_GATE, &ctx, false, NULL);
    mode_handler_get_led(MODE_GATE, NULL, &fb);
    mode_handler_get_led(MODE_GATE, &ctx, NULL);

    TEST_PASS();
}

// =============================================================================
// Test Group Runner
// =============================================================================

TEST_GROUP_RUNNER(ModeHandlersTests) {
    // Gate mode
    RUN_TEST_CASE(ModeHandlersTests, TestGateInit);
    RUN_TEST_CASE(ModeHandlersTests, TestGateFollowsInput);
    RUN_TEST_CASE(ModeHandlersTests, TestGateReturnsChangedFlag);

    // Trigger mode
    RUN_TEST_CASE(ModeHandlersTests, TestTriggerInit);
    RUN_TEST_CASE(ModeHandlersTests, TestTriggerPulseOnRisingEdge);
    RUN_TEST_CASE(ModeHandlersTests, TestTriggerPulseExpires);
    RUN_TEST_CASE(ModeHandlersTests, TestTriggerNoRetriggerDuringPulse);

    // Toggle mode
    RUN_TEST_CASE(ModeHandlersTests, TestToggleInit);
    RUN_TEST_CASE(ModeHandlersTests, TestToggleFlipsOnRisingEdge);
    RUN_TEST_CASE(ModeHandlersTests, TestToggleIgnoresHold);

    // Divide mode
    RUN_TEST_CASE(ModeHandlersTests, TestDivideInit);
    RUN_TEST_CASE(ModeHandlersTests, TestDivideByTwo);
    RUN_TEST_CASE(ModeHandlersTests, TestDivideByFour);

    // Cycle mode
    RUN_TEST_CASE(ModeHandlersTests, TestCycleInit);
    RUN_TEST_CASE(ModeHandlersTests, TestCycleOscillates);
    RUN_TEST_CASE(ModeHandlersTests, TestCycleIgnoresInput);
    RUN_TEST_CASE(ModeHandlersTests, TestCycleDefaultBPM);

    // LED feedback
    RUN_TEST_CASE(ModeHandlersTests, TestGateLEDColors);
    RUN_TEST_CASE(ModeHandlersTests, TestTriggerLEDColors);
    RUN_TEST_CASE(ModeHandlersTests, TestLEDActivityReflectsOutput);

    // Null safety
    RUN_TEST_CASE(ModeHandlersTests, TestNullSafety);
}

#endif /* TEST_MODE_HANDLERS_H */
