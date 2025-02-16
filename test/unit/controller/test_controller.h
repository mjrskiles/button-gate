#ifndef TEST_CONTROLLER_H
#define TEST_CONTROLLER_H

#include "unity.h"
#include "unity_fixture.h"
#include "controller/io_controller.h"
#include "hardware/hal_interface.h"

IOController io_controller;
Button button;
CVOutput cv_output;

TEST_GROUP(IOControllerTests);

TEST_SETUP(IOControllerTests) {
    p_hal->init();
    button_init(&button, p_hal->button_pin);
    cv_output_init(&cv_output, p_hal->sig_out_pin);
    io_controller_init(&io_controller, &button, &cv_output, p_hal->led_output_indicator_pin);
}

TEST_TEAR_DOWN(IOControllerTests) {
    button_reset(&button);
    cv_output_reset(&cv_output);
    p_hal->reset_time();
}

TEST(IOControllerTests, TestIOControllerInit) {
    TEST_ASSERT_EQUAL(&button, io_controller.button);
    TEST_ASSERT_EQUAL(&cv_output, io_controller.cv_output);
    TEST_ASSERT_EQUAL(p_hal->led_output_indicator_pin, io_controller.led_pin);
    TEST_ASSERT_EQUAL(MODE_GATE, io_controller.mode);
    TEST_ASSERT_FALSE(io_controller.ignore_pressed);
}

TEST(IOControllerTests, TestIOControllerModeChangeOnConfigAction) {
    // Trigger config action
    button.config_action = true;
    io_controller_update(&io_controller);
    
    TEST_ASSERT_EQUAL(MODE_PULSE, io_controller.mode);
    TEST_ASSERT_TRUE(io_controller.ignore_pressed);
    TEST_ASSERT_FALSE(button.config_action);
}

TEST(IOControllerTests, TestIOControllerGateMode) {
    TEST_ASSERT_EQUAL(MODE_GATE, io_controller.mode);

    // Press button in gate mode
    p_hal->advance_time(100);
    p_hal->set_pin(button.pin);
    io_controller_update(&io_controller);
    
    TEST_ASSERT_TRUE(cv_output.state);
    
    // Release button
    p_hal->clear_pin(button.pin);
    io_controller_update(&io_controller);
    
    TEST_ASSERT_FALSE(cv_output.state);
}

TEST(IOControllerTests, TestIOControllerPulseMode) {
    // Change to pulse mode
    p_hal->set_pin(button.pin);
    p_hal->advance_time(100);
    button.config_action = true;
    io_controller_update(&io_controller);
    TEST_ASSERT_EQUAL(MODE_PULSE, io_controller.mode);

    // Clear the ignore_pressed flag
    p_hal->advance_time(100);
    p_hal->clear_pin(button.pin);
    io_controller_update(&io_controller);
    TEST_ASSERT_FALSE(io_controller.ignore_pressed);
    
    // Trigger pulse
    p_hal->advance_time(100);
    p_hal->set_pin(button.pin);
    io_controller_update(&io_controller);
    TEST_ASSERT_TRUE(cv_output.state);
    
    // Advance past pulse duration
    p_hal->advance_time(PULSE_DURATION_MS + 1);
    io_controller_update(&io_controller);
    TEST_ASSERT_FALSE(cv_output.state);
}

TEST(IOControllerTests, TestIOControllerToggleMode) {
    // Change to toggle mode
    p_hal->set_pin(button.pin);
    p_hal->advance_time(100);
    button.config_action = true;
    io_controller_update(&io_controller);
    button.config_action = true;
    io_controller_update(&io_controller);

    TEST_ASSERT_EQUAL(MODE_TOGGLE, io_controller.mode);

    // Clear the ignore_pressed flag
    p_hal->advance_time(100);
    p_hal->clear_pin(button.pin);
    io_controller_update(&io_controller);
    TEST_ASSERT_FALSE(io_controller.ignore_pressed);
    
    // First toggle on
    p_hal->advance_time(100);
    p_hal->set_pin(button.pin);
    io_controller_update(&io_controller);
    TEST_ASSERT_TRUE(cv_output.state);
    
    // Release and toggle off
    p_hal->advance_time(100);
    p_hal->clear_pin(button.pin);
    io_controller_update(&io_controller);
    p_hal->advance_time(100);
    p_hal->set_pin(button.pin);
    io_controller_update(&io_controller);
    TEST_ASSERT_FALSE(cv_output.state);
}

TEST(IOControllerTests, TestIOControllerLEDOutput) {
    // LED should follow CV output state
    p_hal->advance_time(100);
    p_hal->set_pin(button.pin);
    io_controller_update(&io_controller);
    TEST_ASSERT_TRUE(cv_output.state);
    
    p_hal->advance_time(100);
    p_hal->clear_pin(button.pin);
    io_controller_update(&io_controller);
    TEST_ASSERT_FALSE(cv_output.state);
}

TEST(IOControllerTests, TestIOControllerIgnorePressedReset) {
    // Set ignore_pressed via config action
    p_hal->advance_time(100);
    p_hal->set_pin(button.pin);
    button.config_action = true;
    io_controller_update(&io_controller);
    TEST_ASSERT_TRUE(io_controller.ignore_pressed);
    
    // Should reset on button release
    p_hal->advance_time(100);
    p_hal->clear_pin(button.pin);
    io_controller_update(&io_controller);
    TEST_ASSERT_FALSE(io_controller.ignore_pressed);
}

TEST_GROUP_RUNNER(IOControllerTests) {
    RUN_TEST_CASE(IOControllerTests, TestIOControllerInit);
    RUN_TEST_CASE(IOControllerTests, TestIOControllerModeChangeOnConfigAction);
    RUN_TEST_CASE(IOControllerTests, TestIOControllerGateMode);
    RUN_TEST_CASE(IOControllerTests, TestIOControllerPulseMode);
    RUN_TEST_CASE(IOControllerTests, TestIOControllerToggleMode);
    RUN_TEST_CASE(IOControllerTests, TestIOControllerLEDOutput);
    RUN_TEST_CASE(IOControllerTests, TestIOControllerIgnorePressedReset);
}

void RunAllIOControllerTests() {
    RUN_TEST_GROUP(IOControllerTests);
}

#endif // TEST_CONTROLLER_H
