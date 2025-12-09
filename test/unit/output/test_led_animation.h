#ifndef TEST_LED_ANIMATION_H
#define TEST_LED_ANIMATION_H

#include "unity.h"
#include "unity_fixture.h"
#include "output/led_animation.h"
#include "output/neopixel.h"
#include "mocks/mock_neopixel.h"

/**
 * @file test_led_animation.h
 * @brief Unit tests for LED animation engine
 */

TEST_GROUP(LEDAnimationTests);

static LEDAnimation anim;

TEST_SETUP(LEDAnimationTests) {
    mock_neopixel_reset();
    neopixel_init();
    led_animation_init(&anim);
}

TEST_TEAR_DOWN(LEDAnimationTests) {
}

TEST(LEDAnimationTests, TestInit) {
    TEST_ASSERT_EQUAL(ANIM_NONE, anim.type);
    TEST_ASSERT_EQUAL_UINT8(0, anim.base_color.r);
    TEST_ASSERT_EQUAL_UINT8(0, anim.base_color.g);
    TEST_ASSERT_EQUAL_UINT8(0, anim.base_color.b);
}

TEST(LEDAnimationTests, TestSetStatic) {
    NeopixelColor green = {0, 255, 0};
    led_animation_set_static(&anim, green);

    TEST_ASSERT_EQUAL(ANIM_NONE, anim.type);
    TEST_ASSERT_EQUAL_UINT8(0, anim.base_color.r);
    TEST_ASSERT_EQUAL_UINT8(255, anim.base_color.g);
    TEST_ASSERT_EQUAL_UINT8(0, anim.base_color.b);
}

TEST(LEDAnimationTests, TestStaticUpdate) {
    NeopixelColor red = {255, 0, 0};
    led_animation_set_static(&anim, red);

    led_animation_update(&anim, LED_MODE, 0);
    neopixel_flush();

    TEST_ASSERT_TRUE(mock_neopixel_check_color(LED_MODE, 255, 0, 0));
}

TEST(LEDAnimationTests, TestBlinkToggle) {
    NeopixelColor blue = {0, 0, 255};
    led_animation_set(&anim, ANIM_BLINK, blue, 100);  // 100ms period

    // Initially on
    TEST_ASSERT_TRUE(anim.current_on);

    // Update at t=0
    led_animation_update(&anim, LED_MODE, 0);
    neopixel_flush();
    TEST_ASSERT_TRUE(mock_neopixel_check_color(LED_MODE, 0, 0, 255));

    // Update at t=50ms (half period) - should toggle off
    led_animation_update(&anim, LED_MODE, 50);
    neopixel_flush();
    TEST_ASSERT_TRUE(mock_neopixel_check_color(LED_MODE, 0, 0, 0));

    // Update at t=100ms - should toggle on
    led_animation_update(&anim, LED_MODE, 100);
    neopixel_flush();
    TEST_ASSERT_TRUE(mock_neopixel_check_color(LED_MODE, 0, 0, 255));
}

TEST(LEDAnimationTests, TestGlowBrightness) {
    NeopixelColor white = {255, 255, 255};
    led_animation_set(&anim, ANIM_GLOW, white, 1000);  // 1s period

    // At t=0, phase=0, brightness=0 (start of ramp up)
    led_animation_update(&anim, LED_MODE, 0);
    NeopixelColor color0 = neopixel_get_color(LED_MODE);

    // At t=250ms (phase ~64), brightness should be increasing
    led_animation_update(&anim, LED_MODE, 250);
    NeopixelColor color250 = neopixel_get_color(LED_MODE);
    TEST_ASSERT_TRUE(color250.r > color0.r);

    // At t=500ms (phase ~128), brightness should be at max
    led_animation_update(&anim, LED_MODE, 500);
    NeopixelColor color500 = neopixel_get_color(LED_MODE);
    TEST_ASSERT_TRUE(color500.r >= color250.r);

    // At t=750ms, brightness should be decreasing
    led_animation_update(&anim, LED_MODE, 750);
    NeopixelColor color750 = neopixel_get_color(LED_MODE);
    TEST_ASSERT_TRUE(color750.r < color500.r);
}

TEST(LEDAnimationTests, TestStop) {
    NeopixelColor cyan = {0, 255, 255};
    led_animation_set(&anim, ANIM_BLINK, cyan, 500);

    led_animation_stop(&anim, LED_MODE);

    TEST_ASSERT_EQUAL(ANIM_NONE, anim.type);
    TEST_ASSERT_TRUE(mock_neopixel_check_color(LED_MODE, 0, 0, 0));
}

TEST(LEDAnimationTests, TestColorScale) {
    NeopixelColor full = {200, 100, 50};

    // 50% brightness
    NeopixelColor half = led_color_scale(full, 128);
    // Allow small rounding differences
    TEST_ASSERT_INT_WITHIN(2, 100, half.r);
    TEST_ASSERT_INT_WITHIN(2, 50, half.g);
    TEST_ASSERT_INT_WITHIN(2, 25, half.b);

    // 0% brightness
    NeopixelColor zero = led_color_scale(full, 0);
    TEST_ASSERT_EQUAL_UINT8(0, zero.r);
    TEST_ASSERT_EQUAL_UINT8(0, zero.g);
    TEST_ASSERT_EQUAL_UINT8(0, zero.b);

    // 100% brightness
    NeopixelColor same = led_color_scale(full, 255);
    TEST_ASSERT_EQUAL_UINT8(200, same.r);
    TEST_ASSERT_EQUAL_UINT8(100, same.g);
    TEST_ASSERT_EQUAL_UINT8(50, same.b);
}

TEST(LEDAnimationTests, TestNullSafety) {
    // These should not crash
    led_animation_init(NULL);
    led_animation_set(NULL, ANIM_BLINK, (NeopixelColor){0, 0, 0}, 100);
    led_animation_set_static(NULL, (NeopixelColor){0, 0, 0});
    led_animation_update(NULL, LED_MODE, 0);
    led_animation_stop(NULL, LED_MODE);
}

TEST_GROUP_RUNNER(LEDAnimationTests) {
    RUN_TEST_CASE(LEDAnimationTests, TestInit);
    RUN_TEST_CASE(LEDAnimationTests, TestSetStatic);
    RUN_TEST_CASE(LEDAnimationTests, TestStaticUpdate);
    RUN_TEST_CASE(LEDAnimationTests, TestBlinkToggle);
    RUN_TEST_CASE(LEDAnimationTests, TestGlowBrightness);
    RUN_TEST_CASE(LEDAnimationTests, TestStop);
    RUN_TEST_CASE(LEDAnimationTests, TestColorScale);
    RUN_TEST_CASE(LEDAnimationTests, TestNullSafety);
}

void RunAllLEDAnimationTests(void) {
    RUN_TEST_GROUP(LEDAnimationTests);
}

#endif /* TEST_LED_ANIMATION_H */
