#ifndef GK_TEST_CONTROLLER_H
#define GK_TEST_CONTROLLER_H

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

// P2: LED pin verification tests

TEST(IOControllerTests, TestOutputIndicatorLEDFollowsCVOutput) {
    // In gate mode, LED should follow CV output
    p_hal->advance_time(100);
    p_hal->set_pin(button.pin);
    io_controller_update(&io_controller);

    // Verify both cv_output state AND actual HAL pin state
    TEST_ASSERT_TRUE(cv_output.state);
    TEST_ASSERT_EQUAL(1, p_hal->read_pin(p_hal->led_output_indicator_pin));

    // Release and verify LED turns off
    p_hal->advance_time(100);
    p_hal->clear_pin(button.pin);
    io_controller_update(&io_controller);

    TEST_ASSERT_FALSE(cv_output.state);
    TEST_ASSERT_EQUAL(0, p_hal->read_pin(p_hal->led_output_indicator_pin));
}

TEST(IOControllerTests, TestModeLEDsInGateMode) {
    // Gate mode: top=ON, bottom=OFF
    io_controller_update(&io_controller);

    TEST_ASSERT_EQUAL(MODE_GATE, io_controller.mode);
    TEST_ASSERT_EQUAL(1, p_hal->read_pin(p_hal->led_mode_top_pin));
    TEST_ASSERT_EQUAL(0, p_hal->read_pin(p_hal->led_mode_bottom_pin));
}

TEST(IOControllerTests, TestModeLEDsInPulseMode) {
    // Switch to pulse mode
    button.config_action = true;
    io_controller_update(&io_controller);

    // Pulse mode: top=OFF, bottom=ON
    TEST_ASSERT_EQUAL(MODE_PULSE, io_controller.mode);
    TEST_ASSERT_EQUAL(0, p_hal->read_pin(p_hal->led_mode_top_pin));
    TEST_ASSERT_EQUAL(1, p_hal->read_pin(p_hal->led_mode_bottom_pin));
}

TEST(IOControllerTests, TestModeLEDsInToggleMode) {
    // Switch to toggle mode (two mode changes)
    button.config_action = true;
    io_controller_update(&io_controller);
    button.config_action = true;
    io_controller_update(&io_controller);

    // Toggle mode: top=ON, bottom=ON
    TEST_ASSERT_EQUAL(MODE_TOGGLE, io_controller.mode);
    TEST_ASSERT_EQUAL(1, p_hal->read_pin(p_hal->led_mode_top_pin));
    TEST_ASSERT_EQUAL(1, p_hal->read_pin(p_hal->led_mode_bottom_pin));
}

TEST(IOControllerTests, TestCVOutputPinFollowsState) {
    // Verify the actual sig_out pin follows cv_output state
    p_hal->advance_time(100);
    p_hal->set_pin(button.pin);
    io_controller_update(&io_controller);

    TEST_ASSERT_TRUE(cv_output.state);
    TEST_ASSERT_EQUAL(1, p_hal->read_pin(p_hal->sig_out_pin));

    p_hal->advance_time(100);
    p_hal->clear_pin(button.pin);
    io_controller_update(&io_controller);

    TEST_ASSERT_FALSE(cv_output.state);
    TEST_ASSERT_EQUAL(0, p_hal->read_pin(p_hal->sig_out_pin));
}

// P3: Edge case tests

TEST(IOControllerTests, TestCompleteModeCycle) {
    // Test full cycle: Gate -> Pulse -> Toggle -> Gate
    TEST_ASSERT_EQUAL(MODE_GATE, io_controller.mode);

    // Gate -> Pulse
    button.config_action = true;
    io_controller_update(&io_controller);
    TEST_ASSERT_EQUAL(MODE_PULSE, io_controller.mode);

    // Clear ignore_pressed
    p_hal->clear_pin(button.pin);
    io_controller_update(&io_controller);

    // Pulse -> Toggle
    button.config_action = true;
    io_controller_update(&io_controller);
    TEST_ASSERT_EQUAL(MODE_TOGGLE, io_controller.mode);

    // Clear ignore_pressed
    p_hal->clear_pin(button.pin);
    io_controller_update(&io_controller);

    // Toggle -> Gate (full cycle)
    button.config_action = true;
    io_controller_update(&io_controller);
    TEST_ASSERT_EQUAL(MODE_GATE, io_controller.mode);

    // Verify LED states are correct after returning to Gate
    TEST_ASSERT_EQUAL(1, p_hal->read_pin(p_hal->led_mode_top_pin));
    TEST_ASSERT_EQUAL(0, p_hal->read_pin(p_hal->led_mode_bottom_pin));
}

TEST(IOControllerTests, TestToggleModeMultiplePresses) {
    // Switch to toggle mode - need proper button press/release to clear ignore_pressed
    // First mode change: Gate -> Pulse
    p_hal->advance_time(100);
    p_hal->set_pin(button.pin);
    io_controller_update(&io_controller);
    button.config_action = true;
    io_controller_update(&io_controller);
    TEST_ASSERT_EQUAL(MODE_PULSE, io_controller.mode);

    // Release to clear ignore_pressed
    p_hal->advance_time(100);
    p_hal->clear_pin(button.pin);
    io_controller_update(&io_controller);
    TEST_ASSERT_FALSE(io_controller.ignore_pressed);

    // Second mode change: Pulse -> Toggle
    p_hal->advance_time(100);
    p_hal->set_pin(button.pin);
    io_controller_update(&io_controller);
    button.config_action = true;
    io_controller_update(&io_controller);
    TEST_ASSERT_EQUAL(MODE_TOGGLE, io_controller.mode);

    // Release to clear ignore_pressed
    p_hal->advance_time(100);
    p_hal->clear_pin(button.pin);
    io_controller_update(&io_controller);
    TEST_ASSERT_FALSE(io_controller.ignore_pressed);

    // First press: toggle ON
    p_hal->advance_time(100);
    p_hal->set_pin(button.pin);
    io_controller_update(&io_controller);
    TEST_ASSERT_TRUE(cv_output.state);

    // Release
    p_hal->advance_time(100);
    p_hal->clear_pin(button.pin);
    io_controller_update(&io_controller);
    TEST_ASSERT_TRUE(cv_output.state);  // Should stay ON

    // Second press: toggle OFF
    p_hal->advance_time(100);
    p_hal->set_pin(button.pin);
    io_controller_update(&io_controller);
    TEST_ASSERT_FALSE(cv_output.state);

    // Release
    p_hal->advance_time(100);
    p_hal->clear_pin(button.pin);
    io_controller_update(&io_controller);
    TEST_ASSERT_FALSE(cv_output.state);  // Should stay OFF

    // Third press: toggle ON again
    p_hal->advance_time(100);
    p_hal->set_pin(button.pin);
    io_controller_update(&io_controller);
    TEST_ASSERT_TRUE(cv_output.state);
}

TEST_GROUP_RUNNER(IOControllerTests) {
    RUN_TEST_CASE(IOControllerTests, TestIOControllerInit);
    RUN_TEST_CASE(IOControllerTests, TestIOControllerModeChangeOnConfigAction);
    RUN_TEST_CASE(IOControllerTests, TestIOControllerGateMode);
    RUN_TEST_CASE(IOControllerTests, TestIOControllerPulseMode);
    RUN_TEST_CASE(IOControllerTests, TestIOControllerToggleMode);
    RUN_TEST_CASE(IOControllerTests, TestIOControllerLEDOutput);
    RUN_TEST_CASE(IOControllerTests, TestIOControllerIgnorePressedReset);
    // P2: LED pin verification tests
    RUN_TEST_CASE(IOControllerTests, TestOutputIndicatorLEDFollowsCVOutput);
    RUN_TEST_CASE(IOControllerTests, TestModeLEDsInGateMode);
    RUN_TEST_CASE(IOControllerTests, TestModeLEDsInPulseMode);
    RUN_TEST_CASE(IOControllerTests, TestModeLEDsInToggleMode);
    RUN_TEST_CASE(IOControllerTests, TestCVOutputPinFollowsState);
    // P3: Edge case tests
    RUN_TEST_CASE(IOControllerTests, TestCompleteModeCycle);
    RUN_TEST_CASE(IOControllerTests, TestToggleModeMultiplePresses);
}

void RunAllIOControllerTests() {
    RUN_TEST_GROUP(IOControllerTests);
}

#endif /* GK_TEST_CONTROLLER_H */
