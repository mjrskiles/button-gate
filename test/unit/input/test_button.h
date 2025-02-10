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
    mock_hal_init();
    button_init(&button, 2);
}

TEST_TEAR_DOWN(ButtonTests) {
    button_reset(&button);
    reset_mock_time();
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

TEST(ButtonTests, TestButtonReset) {
    button.pressed = true;
    button.last_state = true;
    button.rising_edge = true;
    button.falling_edge = true;
    button.tap_count = 3;
    button.last_rise_time = 1000;
    button.last_fall_time = 2000;
    
    button_reset(&button);
    
    TEST_ASSERT_EQUAL(false, button.raw_state);
    TEST_ASSERT_EQUAL(false, button.pressed);
    TEST_ASSERT_EQUAL(false, button.last_state);
    TEST_ASSERT_EQUAL(false, button.rising_edge);
    TEST_ASSERT_EQUAL(false, button.falling_edge);
    TEST_ASSERT_EQUAL(0, button.tap_count);
    TEST_ASSERT_EQUAL(0, button.last_rise_time);
    TEST_ASSERT_EQUAL(0, button.last_fall_time);
}

TEST(ButtonTests, TestButtonUpdate) {
    // Test rising edge
    mock_set_pin(button.pin);
    advance_mock_time(100);
    button_update(&button);
    TEST_ASSERT_TRUE(button.rising_edge);
    TEST_ASSERT_TRUE(button.pressed);
    
    // Test falling edge
    mock_clear_pin(button.pin);
    advance_mock_time(100);
    button_update(&button);
    TEST_ASSERT_TRUE(button.falling_edge);
    TEST_ASSERT_FALSE(button.pressed);
}

TEST(ButtonTests, TestButtonConfigAction) {
    // Simulate 5 quick taps followed by hold
    for(int i = 0; i < 5; i++) {
        mock_set_pin(button.pin);
        button_update(&button);
        advance_mock_time(100);
        
        mock_clear_pin(button.pin);
        button_update(&button);
        advance_mock_time(100);
    }
    
    // Final press and hold
    mock_set_pin(button.pin);
    button_update(&button);
    advance_mock_time(HOLD_TIME_MS + 100);
    button_update(&button);
    
    TEST_ASSERT_TRUE(button.config_action);
}

TEST(ButtonTests, TestButtonConsumeConfigAction) {
    button.config_action = true;
    button_consume_config_action(&button);
    TEST_ASSERT_FALSE(button.config_action);
}

TEST(ButtonTests, TestButtonHasRisingEdge) {
    mock_set_pin(button.pin);
    advance_mock_time(100);
    button_update(&button);
    TEST_ASSERT_TRUE(button.rising_edge);
    
    // Test debounce
    button_update(&button);
    advance_mock_time(100);
    TEST_ASSERT_FALSE(button.rising_edge);
    
    advance_mock_time(EDGE_DEBOUNCE_MS + 1);
    button_update(&button);
    advance_mock_time(100);
    TEST_ASSERT_FALSE(button.rising_edge);
}

TEST(ButtonTests, TestButtonHasFallingEdge) {
    // First set button to pressed state
    mock_set_pin(button.pin);
    advance_mock_time(100);
    button_update(&button);
    
    // Then test falling edge
    mock_clear_pin(button.pin);
    advance_mock_time(100);
    button_update(&button);
    TEST_ASSERT_TRUE(button.falling_edge);
    
    // Test debounce
    button_update(&button);
    advance_mock_time(100);
    TEST_ASSERT_FALSE(button.falling_edge);
}

TEST_GROUP_RUNNER(ButtonTests) {
    RUN_TEST_CASE(ButtonTests, TestButtonInit);
    RUN_TEST_CASE(ButtonTests, TestButtonReset);
    RUN_TEST_CASE(ButtonTests, TestButtonUpdate);
    RUN_TEST_CASE(ButtonTests, TestButtonConfigAction);
    RUN_TEST_CASE(ButtonTests, TestButtonConsumeConfigAction);
    RUN_TEST_CASE(ButtonTests, TestButtonHasRisingEdge);
    RUN_TEST_CASE(ButtonTests, TestButtonHasFallingEdge);
}

void RunAllButtonTests() {
    RUN_TEST_GROUP(ButtonTests);
}

#endif // TEST_BUTTON_H
