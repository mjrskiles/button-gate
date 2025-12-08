#ifndef GK_TEST_STRUCT_SIZES_H
#define GK_TEST_STRUCT_SIZES_H

#include "unity.h"
#include "unity_fixture.h"
#include "input/button.h"
#include "output/cv_output.h"

/**
 * @file test_struct_sizes.h
 * @brief Verify struct layouts after bitmask consolidation (ADR-002)
 *
 * These tests verify the bitmask consolidation is in place.
 *
 * Note: Actual struct sizes vary by platform due to alignment:
 *   - ATtiny85 (8-bit, no padding): Button=15, CVOutput=6
 *   - Host (32/64-bit, with padding): Button=16, CVOutput=8
 *
 * The key verification is that status words exist and bit layouts are correct.
 * Memory savings on target: ~8 bytes (Button: 6, CVOutput: 2)
 */

TEST_GROUP(StructSizeTests);

TEST_SETUP(StructSizeTests) {
}

TEST_TEAR_DOWN(StructSizeTests) {
}

TEST(StructSizeTests, TestButtonHasStatusField) {
    // Verify Button struct has status field (bitmask consolidation in place)
    Button btn;
    btn.status = 0;
    STATUS_SET(btn.status, BTN_PRESSED);
    TEST_ASSERT_TRUE(STATUS_ANY(btn.status, BTN_PRESSED));
}

TEST(StructSizeTests, TestCVOutputHasStatusField) {
    // Verify CVOutput struct has status field (bitmask consolidation in place)
    CVOutput cv;
    cv.status = 0;
    STATUS_SET(cv.status, CVOUT_STATE);
    TEST_ASSERT_TRUE(STATUS_ANY(cv.status, CVOUT_STATE));
}

TEST(StructSizeTests, TestButtonStatusBitLayout) {
    // Verify bit positions don't overlap and are powers of 2
    TEST_ASSERT_EQUAL(0x01, BTN_RAW);
    TEST_ASSERT_EQUAL(0x02, BTN_PRESSED);
    TEST_ASSERT_EQUAL(0x04, BTN_LAST);
    TEST_ASSERT_EQUAL(0x08, BTN_RISE);
    TEST_ASSERT_EQUAL(0x10, BTN_FALL);
    TEST_ASSERT_EQUAL(0x20, BTN_CONFIG);
    TEST_ASSERT_EQUAL(0x40, BTN_COUNTING);

    // Verify no overlap (all bits combined should equal sum)
    uint8_t all_bits = BTN_RAW | BTN_PRESSED | BTN_LAST | BTN_RISE |
                       BTN_FALL | BTN_CONFIG | BTN_COUNTING;
    TEST_ASSERT_EQUAL(0x7F, all_bits);
}

TEST(StructSizeTests, TestCVOutputStatusBitLayout) {
    // Verify bit positions don't overlap and are powers of 2
    TEST_ASSERT_EQUAL(0x01, CVOUT_STATE);
    TEST_ASSERT_EQUAL(0x02, CVOUT_PULSE);
    TEST_ASSERT_EQUAL(0x04, CVOUT_LAST_IN);

    // Verify no overlap
    uint8_t all_bits = CVOUT_STATE | CVOUT_PULSE | CVOUT_LAST_IN;
    TEST_ASSERT_EQUAL(0x07, all_bits);
}

TEST(StructSizeTests, TestStatusMacrosWork) {
    uint8_t status = 0;

    // Test SET
    STATUS_SET(status, 0x05);
    TEST_ASSERT_EQUAL(0x05, status);

    // Test CLR
    STATUS_CLR(status, 0x01);
    TEST_ASSERT_EQUAL(0x04, status);

    // Test ANY
    TEST_ASSERT_TRUE(STATUS_ANY(status, 0x04));
    TEST_ASSERT_FALSE(STATUS_ANY(status, 0x01));

    // Test ALL
    status = 0x07;
    TEST_ASSERT_TRUE(STATUS_ALL(status, 0x03));
    TEST_ASSERT_FALSE(STATUS_ALL(status, 0x0F));

    // Test NONE
    TEST_ASSERT_TRUE(STATUS_NONE(status, 0x08));
    TEST_ASSERT_FALSE(STATUS_NONE(status, 0x01));

    // Test PUT
    STATUS_PUT(status, 0x08, 1);
    TEST_ASSERT_EQUAL(0x0F, status);
    STATUS_PUT(status, 0x08, 0);
    TEST_ASSERT_EQUAL(0x07, status);
}

TEST_GROUP_RUNNER(StructSizeTests) {
    RUN_TEST_CASE(StructSizeTests, TestButtonHasStatusField);
    RUN_TEST_CASE(StructSizeTests, TestCVOutputHasStatusField);
    RUN_TEST_CASE(StructSizeTests, TestButtonStatusBitLayout);
    RUN_TEST_CASE(StructSizeTests, TestCVOutputStatusBitLayout);
    RUN_TEST_CASE(StructSizeTests, TestStatusMacrosWork);
}

void RunAllStructSizeTests() {
    RUN_TEST_GROUP(StructSizeTests);
}

#endif /* GK_TEST_STRUCT_SIZES_H */
