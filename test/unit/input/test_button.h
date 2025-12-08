#ifndef GK_TEST_INPUT_BUTTON_H
#define GK_TEST_INPUT_BUTTON_H

#include "unity.h"
#include "unity_fixture.h"
#include "input/button.h"
#include "hardware/hal_interface.h"
#include "utility/status.h"

Button button;

TEST_GROUP(ButtonTests);

TEST_SETUP(ButtonTests) {
    p_hal->init();
    button_init(&button, 2);
}

TEST_TEAR_DOWN(ButtonTests) {
    button_reset(&button);
    p_hal->reset_time();
}

TEST(ButtonTests, TestButtonInit) {
    TEST_ASSERT_EQUAL(true, button_init(&button, 2));
    TEST_ASSERT_EQUAL(2, button.pin);
    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_PRESSED));
    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_LAST));
    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_RISE));
    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_CONFIG));
    TEST_ASSERT_EQUAL(0, button.tap_count);
    TEST_ASSERT_EQUAL(0, button.last_rise_time);
    TEST_ASSERT_EQUAL(0, button.last_fall_time);
}

TEST(ButtonTests, TestButtonReset) {
    STATUS_SET(button.status, BTN_PRESSED | BTN_LAST | BTN_RISE | BTN_FALL);
    button.tap_count = 3;
    button.last_rise_time = 1000;
    button.last_fall_time = 2000;

    button_reset(&button);

    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_RAW));
    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_PRESSED));
    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_LAST));
    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_RISE));
    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_FALL));
    TEST_ASSERT_EQUAL(0, button.tap_count);
    TEST_ASSERT_EQUAL(0, button.last_rise_time);
    TEST_ASSERT_EQUAL(0, button.last_fall_time);
}

TEST(ButtonTests, TestButtonUpdate) {
    // Test rising edge
    p_hal->set_pin(button.pin);
    p_hal->advance_time(100);
    button_update(&button);
    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_RISE));
    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_PRESSED));

    // Test falling edge
    p_hal->clear_pin(button.pin);
    p_hal->advance_time(100);
    button_update(&button);
    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_FALL));
    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_PRESSED));
}

TEST(ButtonTests, TestButtonConfigAction) {
    // Advance past time 0 to avoid edge case
    p_hal->advance_time(100);

    // Do 4 quick taps (tap and release)
    for (int i = 0; i < 4; i++) {
        p_hal->set_pin(button.pin);
        button_update(&button);
        p_hal->advance_time(100);

        p_hal->clear_pin(button.pin);
        button_update(&button);
        p_hal->advance_time(100);
    }

    // 5th tap: press and HOLD (don't release)
    p_hal->set_pin(button.pin);
    button_update(&button);
    TEST_ASSERT_EQUAL(5, button.tap_count);
    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_COUNTING));

    // Hold for required time
    p_hal->advance_time(HOLD_TIME_MS + 100);
    button_update(&button);

    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_CONFIG));
}

TEST(ButtonTests, TestButtonConsumeConfigAction) {
    STATUS_SET(button.status, BTN_CONFIG);
    button_consume_config_action(&button);
    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_CONFIG));
}

TEST(ButtonTests, TestButtonHasRisingEdge) {
    p_hal->set_pin(button.pin);
    p_hal->advance_time(100);
    button_update(&button);
    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_RISE));

    // Test debounce
    button_update(&button);
    p_hal->advance_time(100);
    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_RISE));

    p_hal->advance_time(EDGE_DEBOUNCE_MS + 1);
    button_update(&button);
    p_hal->advance_time(100);
    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_RISE));
}

TEST(ButtonTests, TestButtonHasFallingEdge) {
    // First set button to pressed state
    p_hal->set_pin(button.pin);
    p_hal->advance_time(100);
    button_update(&button);

    // Then test falling edge
    p_hal->clear_pin(button.pin);
    p_hal->advance_time(100);
    button_update(&button);
    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_FALL));

    // Test debounce
    button_update(&button);
    p_hal->advance_time(100);
    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_FALL));
}

TEST(ButtonTests, TestConfigActionTapTimeoutResetsTapCount) {
    // Advance past time 0 to avoid edge case with initial last_tap_time
    p_hal->advance_time(100);

    // Do 3 quick taps
    for (int i = 0; i < 3; i++) {
        p_hal->set_pin(button.pin);
        button_update(&button);
        p_hal->advance_time(100);

        p_hal->clear_pin(button.pin);
        button_update(&button);
        p_hal->advance_time(100);
    }
    TEST_ASSERT_EQUAL(3, button.tap_count);

    // Wait longer than TAP_TIMEOUT_MS without pressing
    p_hal->advance_time(TAP_TIMEOUT_MS + 100);
    button_update(&button);

    // Tap count should reset to 0
    TEST_ASSERT_EQUAL(0, button.tap_count);
}

TEST(ButtonTests, TestConfigActionFailsWhenTapsTooSlow) {
    // Advance past time 0 to avoid edge case with initial last_tap_time
    p_hal->advance_time(100);

    // Do 5 taps, but with too much delay between them
    for (int i = 0; i < 5; i++) {
        p_hal->set_pin(button.pin);
        button_update(&button);
        p_hal->advance_time(100);

        p_hal->clear_pin(button.pin);
        button_update(&button);

        // Wait longer than timeout between taps
        p_hal->advance_time(TAP_TIMEOUT_MS + 100);
        button_update(&button);
    }

    // Tap count should be 0 (timeout resets it when button is released and time passes)
    TEST_ASSERT_EQUAL(0, button.tap_count);

    // Now try to hold - should NOT trigger config action since we don't have 5 taps
    p_hal->set_pin(button.pin);
    button_update(&button);
    p_hal->advance_time(HOLD_TIME_MS + 100);
    button_update(&button);

    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_CONFIG));
}

TEST(ButtonTests, TestConfigActionSucceedsWithFastTaps) {
    // Advance past time 0 to avoid edge case with initial last_tap_time
    p_hal->advance_time(100);

    // Do 4 quick taps within timeout window (tap and release)
    for (int i = 0; i < 4; i++) {
        p_hal->set_pin(button.pin);
        button_update(&button);
        p_hal->advance_time(50);  // 50ms tap

        p_hal->clear_pin(button.pin);
        button_update(&button);
        p_hal->advance_time(50);  // 50ms between taps (well under 500ms timeout)
    }

    TEST_ASSERT_EQUAL(4, button.tap_count);

    // 5th tap: press and hold to trigger config action
    p_hal->set_pin(button.pin);
    button_update(&button);
    TEST_ASSERT_EQUAL(5, button.tap_count);
    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_COUNTING));

    p_hal->advance_time(HOLD_TIME_MS + 100);
    button_update(&button);

    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_CONFIG));
}

TEST(ButtonTests, TestConfigActionFailsIfReleasedBeforeHold) {
    // Advance past time 0 to avoid edge case
    p_hal->advance_time(100);

    // Do 4 quick taps
    for (int i = 0; i < 4; i++) {
        p_hal->set_pin(button.pin);
        button_update(&button);
        p_hal->advance_time(50);

        p_hal->clear_pin(button.pin);
        button_update(&button);
        p_hal->advance_time(50);
    }

    // 5th tap: press but release before hold time
    p_hal->set_pin(button.pin);
    button_update(&button);
    TEST_ASSERT_EQUAL(5, button.tap_count);
    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_COUNTING));

    // Release before hold time completes
    p_hal->advance_time(HOLD_TIME_MS / 2);
    p_hal->clear_pin(button.pin);
    button_update(&button);

    // Should NOT trigger config action
    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_CONFIG));
    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_COUNTING));

    // Tap count should still be 5 (can retry)
    TEST_ASSERT_EQUAL(5, button.tap_count);
}

// P1: Boundary condition tests

TEST(ButtonTests, TestHoldTriggersAtExactBoundary) {
    p_hal->advance_time(100);

    // Do 4 quick taps
    for (int i = 0; i < 4; i++) {
        p_hal->set_pin(button.pin);
        button_update(&button);
        p_hal->advance_time(50);
        p_hal->clear_pin(button.pin);
        button_update(&button);
        p_hal->advance_time(50);
    }

    // 5th tap and hold for exactly HOLD_TIME_MS
    p_hal->set_pin(button.pin);
    button_update(&button);

    p_hal->advance_time(HOLD_TIME_MS);  // Exactly 1000ms
    button_update(&button);

    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_CONFIG));
}

TEST(ButtonTests, TestHoldDoesNotTriggerJustBeforeBoundary) {
    p_hal->advance_time(100);

    // Do 4 quick taps
    for (int i = 0; i < 4; i++) {
        p_hal->set_pin(button.pin);
        button_update(&button);
        p_hal->advance_time(50);
        p_hal->clear_pin(button.pin);
        button_update(&button);
        p_hal->advance_time(50);
    }

    // 5th tap and hold for HOLD_TIME_MS - 1
    p_hal->set_pin(button.pin);
    button_update(&button);

    p_hal->advance_time(HOLD_TIME_MS - 1);  // 999ms
    button_update(&button);

    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_CONFIG));
}

TEST(ButtonTests, TestTapTimeoutAtExactBoundary) {
    p_hal->advance_time(100);

    // First tap at time 100
    p_hal->set_pin(button.pin);
    button_update(&button);  // Rising edge at time 100, last_tap_time = 100
    TEST_ASSERT_EQUAL(1, button.tap_count);

    p_hal->advance_time(50);
    p_hal->clear_pin(button.pin);
    button_update(&button);  // Time is 150

    // Wait exactly TAP_TIMEOUT_MS from the first tap (time 100)
    // So next tap at time 100 + 500 = 600 means diff = 500 which is <= 500
    p_hal->advance_time(TAP_TIMEOUT_MS - 50);  // Time is now 600
    p_hal->set_pin(button.pin);
    button_update(&button);

    // At exactly TAP_TIMEOUT_MS from first tap, should still count (uses <=)
    TEST_ASSERT_EQUAL(2, button.tap_count);
}

TEST(ButtonTests, TestTapTimeoutJustAfterBoundary) {
    p_hal->advance_time(100);

    // First tap
    p_hal->set_pin(button.pin);
    button_update(&button);
    p_hal->advance_time(50);
    p_hal->clear_pin(button.pin);
    button_update(&button);

    TEST_ASSERT_EQUAL(1, button.tap_count);

    // Wait TAP_TIMEOUT_MS + 1, then tap again
    p_hal->advance_time(TAP_TIMEOUT_MS + 1);
    p_hal->set_pin(button.pin);
    button_update(&button);

    // Just past timeout, should reset to 1
    TEST_ASSERT_EQUAL(1, button.tap_count);
}

TEST(ButtonTests, TestDebounceBoundaryExact) {
    p_hal->advance_time(100);

    // First rising edge
    p_hal->set_pin(button.pin);
    button_update(&button);
    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_RISE));

    // Release
    p_hal->clear_pin(button.pin);
    button_update(&button);

    // Try another press at exactly EDGE_DEBOUNCE_MS (should NOT trigger - uses >)
    p_hal->advance_time(EDGE_DEBOUNCE_MS);
    p_hal->set_pin(button.pin);
    button_update(&button);
    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_RISE));
}

TEST(ButtonTests, TestDebounceBoundaryJustAfter) {
    p_hal->advance_time(100);

    // First rising edge
    p_hal->set_pin(button.pin);
    button_update(&button);
    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_RISE));

    // Release
    p_hal->clear_pin(button.pin);
    button_update(&button);

    // Try another press at EDGE_DEBOUNCE_MS + 1 (should trigger)
    p_hal->advance_time(EDGE_DEBOUNCE_MS + 1);
    p_hal->set_pin(button.pin);
    button_update(&button);
    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_RISE));
}

// P3: Edge case tests

TEST(ButtonTests, TestBounceNotDetectedAsMultipleRisingEdges) {
    p_hal->advance_time(100);

    // Press button
    p_hal->set_pin(button.pin);
    button_update(&button);
    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_RISE));
    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_PRESSED));

    // Simulate bounce: rapid release and re-press within debounce window
    // The first falling edge WILL be detected (debounce only prevents rapid re-triggers)
    p_hal->advance_time(1);  // 1ms
    p_hal->clear_pin(button.pin);
    button_update(&button);
    // First falling edge IS detected (last_fall_time was 0)

    // Bounce back to pressed quickly - THIS should be debounced
    p_hal->advance_time(1);  // 2ms total, within 5ms debounce of last rising edge
    p_hal->set_pin(button.pin);
    button_update(&button);
    TEST_ASSERT_FALSE(STATUS_ANY(button.status, BTN_RISE));  // Should NOT detect - too soon after last rising edge

    // Wait past debounce, then release and re-press
    p_hal->advance_time(EDGE_DEBOUNCE_MS + 1);
    p_hal->clear_pin(button.pin);
    button_update(&button);
    p_hal->advance_time(1);
    p_hal->set_pin(button.pin);
    button_update(&button);
    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_RISE));  // Now it SHOULD detect - debounce window passed
}

TEST(ButtonTests, TestConfigActionCanRetriggerAfterCompletion) {
    p_hal->advance_time(100);

    // First config action
    for (int i = 0; i < 4; i++) {
        p_hal->set_pin(button.pin);
        button_update(&button);
        p_hal->advance_time(50);
        p_hal->clear_pin(button.pin);
        button_update(&button);
        p_hal->advance_time(50);
    }
    p_hal->set_pin(button.pin);
    button_update(&button);
    p_hal->advance_time(HOLD_TIME_MS);
    button_update(&button);
    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_CONFIG));

    // Consume and release
    button_consume_config_action(&button);
    p_hal->clear_pin(button.pin);
    button_update(&button);

    // Wait for timeout to fully reset
    p_hal->advance_time(TAP_TIMEOUT_MS + 100);
    button_update(&button);
    TEST_ASSERT_EQUAL(0, button.tap_count);

    // Second config action should work
    for (int i = 0; i < 4; i++) {
        p_hal->set_pin(button.pin);
        button_update(&button);
        p_hal->advance_time(50);
        p_hal->clear_pin(button.pin);
        button_update(&button);
        p_hal->advance_time(50);
    }
    p_hal->set_pin(button.pin);
    button_update(&button);
    p_hal->advance_time(HOLD_TIME_MS);
    button_update(&button);
    TEST_ASSERT_TRUE(STATUS_ANY(button.status, BTN_CONFIG));
}

TEST_GROUP_RUNNER(ButtonTests) {
    RUN_TEST_CASE(ButtonTests, TestButtonInit);
    RUN_TEST_CASE(ButtonTests, TestButtonReset);
    RUN_TEST_CASE(ButtonTests, TestButtonUpdate);
    RUN_TEST_CASE(ButtonTests, TestButtonConfigAction);
    RUN_TEST_CASE(ButtonTests, TestButtonConsumeConfigAction);
    RUN_TEST_CASE(ButtonTests, TestButtonHasRisingEdge);
    RUN_TEST_CASE(ButtonTests, TestButtonHasFallingEdge);
    RUN_TEST_CASE(ButtonTests, TestConfigActionTapTimeoutResetsTapCount);
    RUN_TEST_CASE(ButtonTests, TestConfigActionFailsWhenTapsTooSlow);
    RUN_TEST_CASE(ButtonTests, TestConfigActionSucceedsWithFastTaps);
    RUN_TEST_CASE(ButtonTests, TestConfigActionFailsIfReleasedBeforeHold);
    // P1: Boundary condition tests
    RUN_TEST_CASE(ButtonTests, TestHoldTriggersAtExactBoundary);
    RUN_TEST_CASE(ButtonTests, TestHoldDoesNotTriggerJustBeforeBoundary);
    RUN_TEST_CASE(ButtonTests, TestTapTimeoutAtExactBoundary);
    RUN_TEST_CASE(ButtonTests, TestTapTimeoutJustAfterBoundary);
    RUN_TEST_CASE(ButtonTests, TestDebounceBoundaryExact);
    RUN_TEST_CASE(ButtonTests, TestDebounceBoundaryJustAfter);
    // P3: Edge case tests
    RUN_TEST_CASE(ButtonTests, TestBounceNotDetectedAsMultipleRisingEdges);
    RUN_TEST_CASE(ButtonTests, TestConfigActionCanRetriggerAfterCompletion);
}

void RunAllButtonTests() {
    RUN_TEST_GROUP(ButtonTests);
}

#endif /* GK_TEST_INPUT_BUTTON_H */
