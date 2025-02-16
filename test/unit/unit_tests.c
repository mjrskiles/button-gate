#include "unity_fixture.h"

#include "mocks/mock_hal.h"

#include "example/test_example.h"
#include "input/test_button.h"
#include "output/test_cv_output.h"
#include "state/test_mode.h"
#include "controller/test_controller.h"

void run_all_tests(void);

int main(void) {
    use_mock_hal();
    
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
    RunAllCVOutputTests();
    RunAllModeTests();
    RunAllIOControllerTests();
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