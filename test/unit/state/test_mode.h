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

TEST(ModeTests, TestModeTransitionFromGate) {
    CVMode next_mode = cv_mode_get_next(MODE_GATE);
    TEST_ASSERT_EQUAL(MODE_PULSE, next_mode);
}

TEST(ModeTests, TestModeTransitionFromPulse) {
    CVMode next_mode = cv_mode_get_next(MODE_PULSE); 
    TEST_ASSERT_EQUAL(MODE_TOGGLE, next_mode);
}

TEST(ModeTests, TestModeTransitionFromToggle) {
    CVMode next_mode = cv_mode_get_next(MODE_TOGGLE);
    TEST_ASSERT_EQUAL(MODE_GATE, next_mode);
}

TEST(ModeTests, TestModeTransitionFromInvalid) {
    CVMode next_mode = cv_mode_get_next(99); // Invalid mode
    TEST_ASSERT_EQUAL(MODE_GATE, next_mode);
}

TEST(ModeTests, TestModeLEDStateGate) {
    ModeLEDState state = cv_mode_get_led_state(MODE_GATE);
    TEST_ASSERT_TRUE(state.top);
    TEST_ASSERT_FALSE(state.bottom);
}

TEST(ModeTests, TestModeLEDStatePulse) {
    ModeLEDState state = cv_mode_get_led_state(MODE_PULSE);
    TEST_ASSERT_FALSE(state.top);
    TEST_ASSERT_TRUE(state.bottom);
}

TEST(ModeTests, TestModeLEDStateToggle) {
    ModeLEDState state = cv_mode_get_led_state(MODE_TOGGLE);
    TEST_ASSERT_TRUE(state.top);
    TEST_ASSERT_TRUE(state.bottom);
}

TEST(ModeTests, TestModeLEDStateInvalid) {
    ModeLEDState state = cv_mode_get_led_state(99); // Invalid mode defaults to Gate
    TEST_ASSERT_TRUE(state.top);
    TEST_ASSERT_FALSE(state.bottom);
}

TEST_GROUP_RUNNER(ModeTests) {
    RUN_TEST_CASE(ModeTests, TestModeTransitionFromGate);
    RUN_TEST_CASE(ModeTests, TestModeTransitionFromPulse);
    RUN_TEST_CASE(ModeTests, TestModeTransitionFromToggle);
    RUN_TEST_CASE(ModeTests, TestModeTransitionFromInvalid);
    RUN_TEST_CASE(ModeTests, TestModeLEDStateGate);
    RUN_TEST_CASE(ModeTests, TestModeLEDStatePulse);
    RUN_TEST_CASE(ModeTests, TestModeLEDStateToggle);
    RUN_TEST_CASE(ModeTests, TestModeLEDStateInvalid);
}

void RunAllModeTests() {
    RUN_TEST_GROUP(ModeTests);
}

#endif /* GK_TEST_STATE_MODE_H */
