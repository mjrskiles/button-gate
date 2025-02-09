#ifndef TEST_EXAMPLE_H
#define TEST_EXAMPLE_H

#include "unity.h"
#include "unity_fixture.h"

TEST_GROUP(ExampleTests);

TEST_SETUP(ExampleTests) {
    // Setup code, if needed
}

TEST_TEAR_DOWN(ExampleTests) {
    // Teardown code, if needed
}

TEST(ExampleTests, SimpleAssertion) {
    TEST_ASSERT_EQUAL(1, 1);
}

TEST_GROUP_RUNNER(ExampleTests) {
    RUN_TEST_CASE(ExampleTests, SimpleAssertion);
}

void RunAllExampleTests() {
    RUN_TEST_GROUP(ExampleTests);
}

#endif // TEST_EXAMPLE_H
