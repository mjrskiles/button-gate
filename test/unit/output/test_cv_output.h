#ifndef TEST_CV_OUTPUT_H
#define TEST_CV_OUTPUT_H

#include "unity.h"
#include "unity_fixture.h"
#include "output/cv_output.h"
#include "mocks/mock_hal.h"

CVOutput cv_output;

TEST_GROUP(CVOutputTests);

TEST_SETUP(CVOutputTests) {
    use_mock_hal();
    mock_hal_init();
    cv_output_init(&cv_output, 1);
}

TEST_TEAR_DOWN(CVOutputTests) {
    cv_output_reset(&cv_output);
    reset_mock_time();

    // Release the button and make sure the static variables are reset
    cv_output_update_pulse(&cv_output, false);
    cv_output_update_toggle(&cv_output, false);
}

TEST(CVOutputTests, TestCVOutputInit) {
    cv_output_init(&cv_output, 1);
    TEST_ASSERT_EQUAL(1, cv_output.pin);
    TEST_ASSERT_EQUAL(false, cv_output.state);
}

TEST(CVOutputTests, TestCVOutputReset) {
    cv_output.state = true;
    cv_output_reset(&cv_output);
    TEST_ASSERT_EQUAL(false, cv_output.state);
}

TEST(CVOutputTests, TestCVOutputSet) {
    cv_output_set(&cv_output);
    TEST_ASSERT_TRUE(cv_output.state);
}

TEST(CVOutputTests, TestCVOutputClear) {
    cv_output.state = true;
    cv_output_clear(&cv_output);
    TEST_ASSERT_FALSE(cv_output.state);
}

TEST(CVOutputTests, TestCVOutputUpdateGateHigh) {
    bool result = cv_output_update_gate(&cv_output, true);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(cv_output.state);
}

TEST(CVOutputTests, TestCVOutputUpdateGateLow) {
    cv_output.state = true;
    bool result = cv_output_update_gate(&cv_output, false);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(cv_output.state);
}

TEST(CVOutputTests, TestCVOutputUpdatePulseRisingEdge) {
    bool result = cv_output_update_pulse(&cv_output, true);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(cv_output.state);

    // Release button
    advance_mock_time(1000);
    cv_output_update_pulse(&cv_output, false);
    TEST_ASSERT_FALSE(cv_output.state);
}

TEST(CVOutputTests, TestCVOutputUpdatePulseDuration) {
    // Trigger pulse
    cv_output_update_pulse(&cv_output, true);
    TEST_ASSERT_TRUE(cv_output.state);
    
    // Advance time just before pulse should end
    advance_mock_time(PULSE_DURATION_MS - 1);
    cv_output_update_pulse(&cv_output, false);
    TEST_ASSERT_TRUE(cv_output.state);
    
    // Advance time past pulse duration
    advance_mock_time(2);
    cv_output_update_pulse(&cv_output, false);
    TEST_ASSERT_FALSE(cv_output.state);
}

TEST(CVOutputTests, TestCVOutputUpdatePulseNoRetrigger) {
    // Initial pulse
    cv_output_update_pulse(&cv_output, true);
    TEST_ASSERT_TRUE(cv_output.state);
    
    // Try to retrigger while pulse is active
    advance_mock_time(PULSE_DURATION_MS / 2);
    cv_output_update_pulse(&cv_output, true);
    TEST_ASSERT_TRUE(cv_output.state);
    
    // Pulse should still end at original time
    advance_mock_time(PULSE_DURATION_MS / 2 + 1);
    cv_output_update_pulse(&cv_output, false);
    TEST_ASSERT_FALSE(cv_output.state);
}

TEST(CVOutputTests, TestCVOutputUpdateToggleRisingEdge) {
    // First toggle
    bool result = cv_output_update_toggle(&cv_output, true);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(cv_output.state);
    
    // Release button
    cv_output_update_toggle(&cv_output, false);
    TEST_ASSERT_TRUE(cv_output.state);
    
    // Second toggle
    result = cv_output_update_toggle(&cv_output, true);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(cv_output.state);
}

TEST(CVOutputTests, TestCVOutputUpdateToggleNoChangeOnHold) {
    // Initial toggle
    cv_output_update_toggle(&cv_output, true);
    TEST_ASSERT_TRUE(cv_output.state);
    
    // Hold button down
    cv_output_update_toggle(&cv_output, true);
    TEST_ASSERT_TRUE(cv_output.state);
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
}

void RunAllCVOutputTests() {
    RUN_TEST_GROUP(CVOutputTests);
}

#endif // TEST_CV_OUTPUT_H
