#ifndef TEST_BUTTON_H
#define TEST_BUTTON_H

#include "unity.h"
#include "unity_fixture.h"
#include "input/button.h"
#include "mocks/mock_hal.h"

Button button;

TEST_GROUP(ButtonTests);

TEST_SETUP(ButtonTests) {
    use_mock_hal();
    button_init(&button, 2);
}

TEST_TEAR_DOWN(ButtonTests) {
// No teardown needed
}

TEST(ButtonTests, TestButtonInit) {
    TEST_ASSERT_EQUAL(true, button_init(&button, 2));
    TEST_ASSERT_EQUAL(2, button.pin);
    TEST_ASSERT_EQUAL(false, button.pressed);
    TEST_ASSERT_EQUAL(false, button.last_state);
    TEST_ASSERT_EQUAL(false, button.rising_edge);
    TEST_ASSERT_EQUAL(false, button.config_action);
    TEST_ASSERT_EQUAL(0, button.tap_count);
    TEST_ASSERT_EQUAL(0, button.last_rise_time);
    TEST_ASSERT_EQUAL(0, button.last_fall_time);
}

TEST_GROUP_RUNNER(ButtonTests) {
    RUN_TEST_CASE(ButtonTests, TestButtonInit);
}

void RunAllButtonTests() {
    RUN_TEST_GROUP(ButtonTests);
}

#endif // TEST_BUTTON_H
