#ifndef GK_TEST_OUTPUT_CV_OUTPUT_H
#define GK_TEST_OUTPUT_CV_OUTPUT_H

#include "unity.h"
#include "unity_fixture.h"
#include "output/cv_output.h"
#include "hardware/hal_interface.h"
#include "utility/status.h"

CVOutput cv_output;

TEST_GROUP(CVOutputTests);

TEST_SETUP(CVOutputTests) {
    p_hal->init();
    cv_output_init(&cv_output, 1);
}

TEST_TEAR_DOWN(CVOutputTests) {
    cv_output_reset(&cv_output);
    p_hal->reset_time();
}

TEST(CVOutputTests, TestCVOutputInit) {
    cv_output_init(&cv_output, 1);
    TEST_ASSERT_EQUAL(1, cv_output.pin);
    TEST_ASSERT_FALSE(STATUS_ANY(cv_output.status, CVOUT_STATE));
}

TEST(CVOutputTests, TestCVOutputReset) {
    STATUS_SET(cv_output.status, CVOUT_STATE);
    cv_output_reset(&cv_output);
    TEST_ASSERT_FALSE(STATUS_ANY(cv_output.status, CVOUT_STATE));
}

TEST(CVOutputTests, TestCVOutputSet) {
    cv_output_set(&cv_output);
    TEST_ASSERT_TRUE(STATUS_ANY(cv_output.status, CVOUT_STATE));
}

TEST(CVOutputTests, TestCVOutputClear) {
    STATUS_SET(cv_output.status, CVOUT_STATE);
    cv_output_clear(&cv_output);
    TEST_ASSERT_FALSE(STATUS_ANY(cv_output.status, CVOUT_STATE));
}

TEST(CVOutputTests, TestCVOutputUpdateGateHigh) {
    bool result = cv_output_update_gate(&cv_output, true);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(STATUS_ANY(cv_output.status, CVOUT_STATE));
}

TEST(CVOutputTests, TestCVOutputUpdateGateLow) {
    STATUS_SET(cv_output.status, CVOUT_STATE);
    bool result = cv_output_update_gate(&cv_output, false);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(STATUS_ANY(cv_output.status, CVOUT_STATE));
}

TEST(CVOutputTests, TestCVOutputUpdatePulseRisingEdge) {
    bool result = cv_output_update_pulse(&cv_output, true);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(STATUS_ANY(cv_output.status, CVOUT_STATE));

    // Release button
    p_hal->advance_time(1000);
    cv_output_update_pulse(&cv_output, false);
    TEST_ASSERT_FALSE(STATUS_ANY(cv_output.status, CVOUT_STATE));
}

TEST(CVOutputTests, TestCVOutputUpdatePulseDuration) {
    // Trigger pulse
    cv_output_update_pulse(&cv_output, true);
    TEST_ASSERT_TRUE(STATUS_ANY(cv_output.status, CVOUT_STATE));

    // Advance time just before pulse should end
    p_hal->advance_time(PULSE_DURATION_MS - 1);
    cv_output_update_pulse(&cv_output, false);
    TEST_ASSERT_TRUE(STATUS_ANY(cv_output.status, CVOUT_STATE));

    // Advance time past pulse duration
    p_hal->advance_time(2);
    cv_output_update_pulse(&cv_output, false);
    TEST_ASSERT_FALSE(STATUS_ANY(cv_output.status, CVOUT_STATE));
}

TEST(CVOutputTests, TestCVOutputUpdatePulseNoRetrigger) {
    // Initial pulse
    cv_output_update_pulse(&cv_output, true);
    TEST_ASSERT_TRUE(STATUS_ANY(cv_output.status, CVOUT_STATE));

    // Try to retrigger while pulse is active
    p_hal->advance_time(PULSE_DURATION_MS / 2);
    cv_output_update_pulse(&cv_output, true);
    TEST_ASSERT_TRUE(STATUS_ANY(cv_output.status, CVOUT_STATE));

    // Pulse should still end at original time
    p_hal->advance_time(PULSE_DURATION_MS / 2 + 1);
    cv_output_update_pulse(&cv_output, false);
    TEST_ASSERT_FALSE(STATUS_ANY(cv_output.status, CVOUT_STATE));
}

TEST(CVOutputTests, TestCVOutputUpdateToggleRisingEdge) {
    // First toggle
    bool result = cv_output_update_toggle(&cv_output, true);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(STATUS_ANY(cv_output.status, CVOUT_STATE));

    // Release button
    cv_output_update_toggle(&cv_output, false);
    TEST_ASSERT_TRUE(STATUS_ANY(cv_output.status, CVOUT_STATE));

    // Second toggle
    result = cv_output_update_toggle(&cv_output, true);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(STATUS_ANY(cv_output.status, CVOUT_STATE));
}

TEST(CVOutputTests, TestCVOutputUpdateToggleNoChangeOnHold) {
    // Initial toggle
    cv_output_update_toggle(&cv_output, true);
    TEST_ASSERT_TRUE(STATUS_ANY(cv_output.status, CVOUT_STATE));

    // Hold button down
    cv_output_update_toggle(&cv_output, true);
    TEST_ASSERT_TRUE(STATUS_ANY(cv_output.status, CVOUT_STATE));
}

// P1: Boundary condition tests

TEST(CVOutputTests, TestPulseExpiresAtExactBoundary) {
    cv_output_update_pulse(&cv_output, true);
    TEST_ASSERT_TRUE(STATUS_ANY(cv_output.status, CVOUT_STATE));
    TEST_ASSERT_TRUE(STATUS_ANY(cv_output.status, CVOUT_PULSE));

    // Advance exactly to pulse duration boundary
    p_hal->advance_time(PULSE_DURATION_MS);
    cv_output_update_pulse(&cv_output, false);

    TEST_ASSERT_FALSE(STATUS_ANY(cv_output.status, CVOUT_STATE));
    TEST_ASSERT_FALSE(STATUS_ANY(cv_output.status, CVOUT_PULSE));
}

TEST(CVOutputTests, TestPulseStillActiveJustBeforeBoundary) {
    cv_output_update_pulse(&cv_output, true);
    TEST_ASSERT_TRUE(STATUS_ANY(cv_output.status, CVOUT_STATE));

    // Advance to just before pulse duration boundary
    p_hal->advance_time(PULSE_DURATION_MS - 1);
    cv_output_update_pulse(&cv_output, false);

    // Should still be active
    TEST_ASSERT_TRUE(STATUS_ANY(cv_output.status, CVOUT_STATE));
    TEST_ASSERT_TRUE(STATUS_ANY(cv_output.status, CVOUT_PULSE));
}

TEST(CVOutputTests, TestPulseImmediateRetriggerAfterExpiry) {
    // First pulse
    cv_output_update_pulse(&cv_output, true);
    p_hal->advance_time(PULSE_DURATION_MS + 1);
    cv_output_update_pulse(&cv_output, false);
    TEST_ASSERT_FALSE(STATUS_ANY(cv_output.status, CVOUT_STATE));

    // Immediate re-trigger
    cv_output_update_pulse(&cv_output, true);
    TEST_ASSERT_TRUE(STATUS_ANY(cv_output.status, CVOUT_STATE));
    TEST_ASSERT_TRUE(STATUS_ANY(cv_output.status, CVOUT_PULSE));

    // Verify second pulse also times out correctly
    p_hal->advance_time(PULSE_DURATION_MS);
    cv_output_update_pulse(&cv_output, false);
    TEST_ASSERT_FALSE(STATUS_ANY(cv_output.status, CVOUT_STATE));
}

TEST_GROUP_RUNNER(CVOutputTests) {
    RUN_TEST_CASE(CVOutputTests, TestCVOutputInit);
    RUN_TEST_CASE(CVOutputTests, TestCVOutputReset);
    RUN_TEST_CASE(CVOutputTests, TestCVOutputSet);
    RUN_TEST_CASE(CVOutputTests, TestCVOutputClear);
    RUN_TEST_CASE(CVOutputTests, TestCVOutputUpdateGateHigh);
    RUN_TEST_CASE(CVOutputTests, TestCVOutputUpdateGateLow);
    RUN_TEST_CASE(CVOutputTests, TestCVOutputUpdatePulseRisingEdge);
    RUN_TEST_CASE(CVOutputTests, TestCVOutputUpdatePulseDuration);
    RUN_TEST_CASE(CVOutputTests, TestCVOutputUpdatePulseNoRetrigger);
    RUN_TEST_CASE(CVOutputTests, TestCVOutputUpdateToggleRisingEdge);
    RUN_TEST_CASE(CVOutputTests, TestCVOutputUpdateToggleNoChangeOnHold);
    // P1: Boundary condition tests
    RUN_TEST_CASE(CVOutputTests, TestPulseExpiresAtExactBoundary);
    RUN_TEST_CASE(CVOutputTests, TestPulseStillActiveJustBeforeBoundary);
    RUN_TEST_CASE(CVOutputTests, TestPulseImmediateRetriggerAfterExpiry);
}

void RunAllCVOutputTests() {
    RUN_TEST_GROUP(CVOutputTests);
}

#endif /* GK_TEST_OUTPUT_CV_OUTPUT_H */
