#ifndef TEST_MODE_H
#define TEST_MODE_H

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

TEST_GROUP_RUNNER(ModeTests) {
    RUN_TEST_CASE(ModeTests, TestModeTransitionFromGate);
    RUN_TEST_CASE(ModeTests, TestModeTransitionFromPulse); 
    RUN_TEST_CASE(ModeTests, TestModeTransitionFromToggle);
    RUN_TEST_CASE(ModeTests, TestModeTransitionFromInvalid);
}

void RunAllModeTests() {
    RUN_TEST_GROUP(ModeTests);
}

#endif // TEST_MODE_H
