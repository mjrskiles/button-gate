#include "unity_fixture.h"

#include "example/test_example.h"
#include "button/test_button.h"

void run_all_tests(void);

int main(void) {

    // Setting the -v flag to print the test results
    const char* argv[] = {
        "button_gate_tests",  
        "-v",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    // Run Unity tests
    return UnityMain(argc, argv, run_all_tests);
}

void run_all_tests(void) {
    RunAllExampleTests();
    RunAllButtonTests();
}

/**
 * @brief Setup the suite
 * implementation of suiteSetUp from unity.h
 */
void suiteSetUp(void) {
}

/**
 * @brief Tear down the suite
 * implementation of suiteTearDown from unity.h
 */
int suiteTearDown(int num_failures) {
    return num_failures;
}