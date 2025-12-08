#ifndef GK_TEST_STATE_MODE_H
#define GK_TEST_STATE_MODE_H

#include "unity.h"
#include "unity_fixture.h"
#include "state/mode.h"

TEST_GROUP(ModeTests);

TEST_SETUP(ModeTests) {
}

TEST_TEAR_DOWN(ModeTests) {
}

// Test mode transitions through the full 5-mode ring
TEST(ModeTests, TestModeTransitionFromGate) {
    CVMode next_mode = cv_mode_get_next(MODE_GATE);
    TEST_ASSERT_EQUAL(MODE_TRIGGER, next_mode);
}

TEST(ModeTests, TestModeTransitionFromTrigger) {
    CVMode next_mode = cv_mode_get_next(MODE_TRIGGER);
    TEST_ASSERT_EQUAL(MODE_TOGGLE, next_mode);
}

TEST(ModeTests, TestModeTransitionFromToggle) {
    CVMode next_mode = cv_mode_get_next(MODE_TOGGLE);
    TEST_ASSERT_EQUAL(MODE_DIVIDE, next_mode);
}

TEST(ModeTests, TestModeTransitionFromDivide) {
    CVMode next_mode = cv_mode_get_next(MODE_DIVIDE);
    TEST_ASSERT_EQUAL(MODE_CYCLE, next_mode);
}

TEST(ModeTests, TestModeTransitionFromCycle) {
    // Cycle wraps back to Gate
    CVMode next_mode = cv_mode_get_next(MODE_CYCLE);
    TEST_ASSERT_EQUAL(MODE_GATE, next_mode);
}

TEST(ModeTests, TestModeTransitionFromInvalid) {
    // Invalid mode wraps via modulo
    CVMode next_mode = cv_mode_get_next(99);
    // 99 % 5 = 4 (CYCLE), next = 0 (GATE)
    TEST_ASSERT_EQUAL(MODE_GATE, next_mode);
}

TEST(ModeTests, TestModeLEDStateGate) {
    ModeLEDState state = cv_mode_get_led_state(MODE_GATE);
    TEST_ASSERT_TRUE(state.top);
    TEST_ASSERT_FALSE(state.bottom);
}

TEST(ModeTests, TestModeLEDStateTrigger) {
    ModeLEDState state = cv_mode_get_led_state(MODE_TRIGGER);
    TEST_ASSERT_FALSE(state.top);
    TEST_ASSERT_TRUE(state.bottom);
}

TEST(ModeTests, TestModeLEDStateToggle) {
    ModeLEDState state = cv_mode_get_led_state(MODE_TOGGLE);
    TEST_ASSERT_TRUE(state.top);
    TEST_ASSERT_TRUE(state.bottom);
}

TEST(ModeTests, TestModeLEDStateDivide) {
    ModeLEDState state = cv_mode_get_led_state(MODE_DIVIDE);
    TEST_ASSERT_FALSE(state.top);
    TEST_ASSERT_FALSE(state.bottom);
}

TEST(ModeTests, TestModeLEDStateCycle) {
    ModeLEDState state = cv_mode_get_led_state(MODE_CYCLE);
    TEST_ASSERT_FALSE(state.top);
    TEST_ASSERT_FALSE(state.bottom);
}

TEST(ModeTests, TestModeLEDStateInvalid) {
    ModeLEDState state = cv_mode_get_led_state(99); // Invalid mode defaults to Gate
    TEST_ASSERT_TRUE(state.top);
    TEST_ASSERT_FALSE(state.bottom);
}

// Legacy alias test
TEST(ModeTests, TestModePulseAlias) {
    // MODE_PULSE should be an alias for MODE_TRIGGER
    TEST_ASSERT_EQUAL(MODE_TRIGGER, MODE_PULSE);
}

TEST_GROUP_RUNNER(ModeTests) {
    RUN_TEST_CASE(ModeTests, TestModeTransitionFromGate);
    RUN_TEST_CASE(ModeTests, TestModeTransitionFromTrigger);
    RUN_TEST_CASE(ModeTests, TestModeTransitionFromToggle);
    RUN_TEST_CASE(ModeTests, TestModeTransitionFromDivide);
    RUN_TEST_CASE(ModeTests, TestModeTransitionFromCycle);
    RUN_TEST_CASE(ModeTests, TestModeTransitionFromInvalid);
    RUN_TEST_CASE(ModeTests, TestModeLEDStateGate);
    RUN_TEST_CASE(ModeTests, TestModeLEDStateTrigger);
    RUN_TEST_CASE(ModeTests, TestModeLEDStateToggle);
    RUN_TEST_CASE(ModeTests, TestModeLEDStateDivide);
    RUN_TEST_CASE(ModeTests, TestModeLEDStateCycle);
    RUN_TEST_CASE(ModeTests, TestModeLEDStateInvalid);
    RUN_TEST_CASE(ModeTests, TestModePulseAlias);
}

void RunAllModeTests() {
    RUN_TEST_GROUP(ModeTests);
}

#endif /* GK_TEST_STATE_MODE_H */
