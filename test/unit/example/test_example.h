/**
 * @file test_example.h
 * @brief Template for creating new unit test groups
 *
 * This file serves as a minimal example for adding new tests to the project.
 * Copy this file to create a new test group and follow these steps:
 *
 * 1. Copy this file to the appropriate subdirectory (e.g., test/unit/mymodule/)
 * 2. Rename to test_mymodule.h
 * 3. Replace "Example" with your module name throughout
 * 4. Add your test cases
 * 5. Register the test group in test/unit/unit_tests.c:
 *    - #include "mymodule/test_mymodule.h"
 *    - Add RunAllMyModuleTests() call in run_all_tests()
 *
 * Unity Fixture Quick Reference:
 * - TEST_SETUP: Runs before each test case (initialize state)
 * - TEST_TEAR_DOWN: Runs after each test case (cleanup)
 * - TEST_GROUP_RUNNER: Lists all test cases to run
 * - RunAll*Tests: Entry point called from unit_tests.c
 *
 * Common Assertions:
 * - TEST_ASSERT_TRUE(condition)
 * - TEST_ASSERT_FALSE(condition)
 * - TEST_ASSERT_EQUAL(expected, actual)
 * - TEST_ASSERT_EQUAL_PTR(expected, actual)
 * - TEST_ASSERT_NULL(pointer)
 * - TEST_ASSERT_NOT_NULL(pointer)
 *
 * Using the Mock HAL:
 * - p_hal->init()           - Reset all pin states
 * - p_hal->set_pin(pin)     - Simulate button press / set output high
 * - p_hal->clear_pin(pin)   - Simulate button release / set output low
 * - p_hal->read_pin(pin)    - Read current pin state
 * - p_hal->advance_time(ms) - Simulate time passing
 * - p_hal->reset_time()     - Reset mock timer to 0
 */

#ifndef TEST_EXAMPLE_H
#define TEST_EXAMPLE_H

#include "unity.h"
#include "unity_fixture.h"

/* Declare the test group */
TEST_GROUP(ExampleTests);

/* Setup: runs before each test case */
TEST_SETUP(ExampleTests) {
    /* Initialize any state needed for tests */
}

/* Teardown: runs after each test case */
TEST_TEAR_DOWN(ExampleTests) {
    /* Clean up any state to ensure test isolation */
}

/* Individual test case */
TEST(ExampleTests, SimpleAssertion) {
    TEST_ASSERT_EQUAL(1, 1);
}

/* Register all test cases in this group */
TEST_GROUP_RUNNER(ExampleTests) {
    RUN_TEST_CASE(ExampleTests, SimpleAssertion);
    /* Add more test cases here:
     * RUN_TEST_CASE(ExampleTests, AnotherTest);
     */
}

/* Entry point called from unit_tests.c */
void RunAllExampleTests(void) {
    RUN_TEST_GROUP(ExampleTests);
}

#endif /* TEST_EXAMPLE_H */
