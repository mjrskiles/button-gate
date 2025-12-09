#ifndef TEST_NEOPIXEL_H
#define TEST_NEOPIXEL_H

#include "unity.h"
#include "unity_fixture.h"
#include "output/neopixel.h"
#include "mocks/mock_neopixel.h"

/**
 * @file test_neopixel.h
 * @brief Unit tests for neopixel driver (mock implementation)
 */

TEST_GROUP(NeopixelTests);

TEST_SETUP(NeopixelTests) {
    mock_neopixel_reset();
}

TEST_TEAR_DOWN(NeopixelTests) {
}

TEST(NeopixelTests, TestInit) {
    neopixel_init();

    // After init, both LEDs should be black
    TEST_ASSERT_TRUE(mock_neopixel_check_color(LED_MODE, 0, 0, 0));
    TEST_ASSERT_TRUE(mock_neopixel_check_color(LED_ACTIVITY, 0, 0, 0));
}

TEST(NeopixelTests, TestSetRGB) {
    neopixel_init();

    neopixel_set_rgb(LED_MODE, 255, 128, 64);

    NeopixelColor color = neopixel_get_color(LED_MODE);
    TEST_ASSERT_EQUAL_UINT8(255, color.r);
    TEST_ASSERT_EQUAL_UINT8(128, color.g);
    TEST_ASSERT_EQUAL_UINT8(64, color.b);
}

TEST(NeopixelTests, TestSetColor) {
    neopixel_init();

    NeopixelColor color = {100, 150, 200};
    neopixel_set_color(LED_ACTIVITY, color);

    TEST_ASSERT_TRUE(mock_neopixel_check_color(LED_ACTIVITY, 100, 150, 200));
}

TEST(NeopixelTests, TestClear) {
    neopixel_init();

    neopixel_set_rgb(LED_MODE, 255, 255, 255);
    neopixel_set_rgb(LED_ACTIVITY, 255, 255, 255);

    neopixel_clear();

    TEST_ASSERT_TRUE(mock_neopixel_check_color(LED_MODE, 0, 0, 0));
    TEST_ASSERT_TRUE(mock_neopixel_check_color(LED_ACTIVITY, 0, 0, 0));
}

TEST(NeopixelTests, TestDirtyFlag) {
    neopixel_init();
    neopixel_flush();

    // After flush, should not be dirty
    TEST_ASSERT_FALSE(neopixel_is_dirty());

    // Setting a color should make it dirty
    neopixel_set_rgb(LED_MODE, 255, 0, 0);
    TEST_ASSERT_TRUE(neopixel_is_dirty());

    // Flush clears dirty flag
    neopixel_flush();
    TEST_ASSERT_FALSE(neopixel_is_dirty());
}

TEST(NeopixelTests, TestFlushIncrementsCount) {
    neopixel_init();

    int initial = mock_neopixel_get_flush_count();

    neopixel_set_rgb(LED_MODE, 1, 2, 3);
    neopixel_flush();

    TEST_ASSERT_EQUAL_INT(initial + 1, mock_neopixel_get_flush_count());
}

TEST(NeopixelTests, TestFlushOnlyWhenDirty) {
    neopixel_init();
    neopixel_flush();  // Clear initial dirty state

    int count_before = mock_neopixel_get_flush_count();

    // Flush without changes - should not increment
    neopixel_flush();
    TEST_ASSERT_EQUAL_INT(count_before, mock_neopixel_get_flush_count());
}

TEST(NeopixelTests, TestInvalidIndex) {
    neopixel_init();

    // Setting invalid index should be ignored (no crash)
    neopixel_set_rgb(99, 255, 255, 255);

    // Getting invalid index should return black
    NeopixelColor color = neopixel_get_color(99);
    TEST_ASSERT_EQUAL_UINT8(0, color.r);
    TEST_ASSERT_EQUAL_UINT8(0, color.g);
    TEST_ASSERT_EQUAL_UINT8(0, color.b);
}

TEST_GROUP_RUNNER(NeopixelTests) {
    RUN_TEST_CASE(NeopixelTests, TestInit);
    RUN_TEST_CASE(NeopixelTests, TestSetRGB);
    RUN_TEST_CASE(NeopixelTests, TestSetColor);
    RUN_TEST_CASE(NeopixelTests, TestClear);
    RUN_TEST_CASE(NeopixelTests, TestDirtyFlag);
    RUN_TEST_CASE(NeopixelTests, TestFlushIncrementsCount);
    RUN_TEST_CASE(NeopixelTests, TestFlushOnlyWhenDirty);
    RUN_TEST_CASE(NeopixelTests, TestInvalidIndex);
}

void RunAllNeopixelTests(void) {
    RUN_TEST_GROUP(NeopixelTests);
}

#endif /* TEST_NEOPIXEL_H */
