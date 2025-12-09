#include "unity_fixture.h"

#include "mocks/mock_hal.h"

#include "example/test_example.h"
#include "input/test_button.h"
#include "output/test_cv_output.h"
#include "output/test_neopixel.h"
#include "output/test_led_animation.h"
#include "output/test_led_feedback.h"
#include "state/test_mode.h"
#include "controller/test_controller.h"
#include "app_init/test_app_init.h"
#include "utility/test_struct_sizes.h"
#include "fsm/test_fsm.h"
#include "fsm/test_events.h"
#include "fsm/test_mode_handlers.h"
#include "core/test_coordinator.h"

void run_all_tests(void);

int main(void) {
    use_mock_hal();
    
    // Setting the -v flag to print the test results
    const char* argv[] = {
        "gatekeeper_tests",
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
    RunAllNeopixelTests();
    RunAllLEDAnimationTests();
    RunAllLEDFeedbackTests();
    RunAllModeTests();
    RunAllIOControllerTests();
    RunAllAppInitTests();
    RunAllStructSizeTests();
    RunAllFSMTests();
    RunAllEventProcessorTests();
    RUN_TEST_GROUP(ModeHandlersTests);
    RunAllCoordinatorTests();
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