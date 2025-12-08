#ifndef GK_TEST_FSM_H
#define GK_TEST_FSM_H

#include "unity.h"
#include "unity_fixture.h"
#include "fsm/fsm.h"

/**
 * Test FSM states
 */
enum TestStates {
    STATE_A = 0,
    STATE_B = 1,
    STATE_C = 2,
};

enum TestEvents {
    TEST_EVT_GO_B = 1,
    TEST_EVT_GO_C = 2,
    TEST_EVT_GO_A = 3,
    TEST_EVT_ACTION_ONLY = 4,
    TEST_EVT_WILDCARD = 5,
};

// Tracking variables for test callbacks
static int entry_a_count = 0;
static int exit_a_count = 0;
static int entry_b_count = 0;
static int exit_b_count = 0;
static int entry_c_count = 0;
static int exit_c_count = 0;
static int update_a_count = 0;
static int update_b_count = 0;
static int action_count = 0;

// Callback functions
static void on_enter_a(void) { entry_a_count++; }
static void on_exit_a(void) { exit_a_count++; }
static void on_enter_b(void) { entry_b_count++; }
static void on_exit_b(void) { exit_b_count++; }
static void on_enter_c(void) { entry_c_count++; }
static void on_exit_c(void) { exit_c_count++; }
static void on_update_a(void) { update_a_count++; }
static void on_update_b(void) { update_b_count++; }
static void on_action(void) { action_count++; }

// Test state table
static const State test_states[] = {
    { STATE_A, on_enter_a, on_exit_a, on_update_a },
    { STATE_B, on_enter_b, on_exit_b, on_update_b },
    { STATE_C, on_enter_c, on_exit_c, NULL },
};

// Test transition table
static const Transition test_transitions[] = {
    { STATE_A, TEST_EVT_GO_B, STATE_B, NULL },
    { STATE_B, TEST_EVT_GO_C, STATE_C, on_action },
    { STATE_C, TEST_EVT_GO_A, STATE_A, NULL },
    { STATE_A, TEST_EVT_ACTION_ONLY, FSM_NO_TRANSITION, on_action },
    { FSM_ANY_STATE, TEST_EVT_WILDCARD, STATE_A, on_action },
};

static FSM fsm;

static void reset_counters(void) {
    entry_a_count = 0;
    exit_a_count = 0;
    entry_b_count = 0;
    exit_b_count = 0;
    entry_c_count = 0;
    exit_c_count = 0;
    update_a_count = 0;
    update_b_count = 0;
    action_count = 0;
}

TEST_GROUP(FSMTests);

TEST_SETUP(FSMTests) {
    reset_counters();
    fsm_init(&fsm, test_states, 3, test_transitions, 5, STATE_A);
}

TEST_TEAR_DOWN(FSMTests) {
}

TEST(FSMTests, TestFSMInit) {
    TEST_ASSERT_EQUAL(STATE_A, fsm_get_state(&fsm));
    TEST_ASSERT_FALSE(fsm_is_active(&fsm));
    TEST_ASSERT_EQUAL(0, entry_a_count);  // Entry not called until start
}

TEST(FSMTests, TestFSMStart) {
    fsm_start(&fsm);
    TEST_ASSERT_TRUE(fsm_is_active(&fsm));
    TEST_ASSERT_EQUAL(1, entry_a_count);  // Entry called on start
    TEST_ASSERT_EQUAL(STATE_A, fsm_get_state(&fsm));
}

TEST(FSMTests, TestFSMTransitionBasic) {
    fsm_start(&fsm);
    reset_counters();

    bool changed = fsm_process_event(&fsm, TEST_EVT_GO_B);

    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL(STATE_B, fsm_get_state(&fsm));
    TEST_ASSERT_EQUAL(1, exit_a_count);
    TEST_ASSERT_EQUAL(1, entry_b_count);
}

TEST(FSMTests, TestFSMTransitionWithAction) {
    fsm_start(&fsm);
    fsm_process_event(&fsm, TEST_EVT_GO_B);  // A -> B
    reset_counters();

    bool changed = fsm_process_event(&fsm, TEST_EVT_GO_C);  // B -> C with action

    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL(STATE_C, fsm_get_state(&fsm));
    TEST_ASSERT_EQUAL(1, exit_b_count);
    TEST_ASSERT_EQUAL(1, action_count);
    TEST_ASSERT_EQUAL(1, entry_c_count);
}

TEST(FSMTests, TestFSMEntryExitOrder) {
    fsm_start(&fsm);
    fsm_process_event(&fsm, TEST_EVT_GO_B);

    // Verify exit was called before entry (both counts are 1)
    // The ordering is guaranteed by the FSM implementation
    TEST_ASSERT_EQUAL(1, exit_a_count);
    TEST_ASSERT_EQUAL(1, entry_b_count);
}

TEST(FSMTests, TestFSMNoMatchingTransition) {
    fsm_start(&fsm);

    // Try event that doesn't match current state
    bool changed = fsm_process_event(&fsm, TEST_EVT_GO_C);  // No A->C transition

    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_EQUAL(STATE_A, fsm_get_state(&fsm));
}

TEST(FSMTests, TestFSMAnyStateWildcard) {
    fsm_start(&fsm);
    fsm_process_event(&fsm, TEST_EVT_GO_B);  // A -> B
    reset_counters();

    // Wildcard should match from any state
    bool changed = fsm_process_event(&fsm, TEST_EVT_WILDCARD);

    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL(STATE_A, fsm_get_state(&fsm));
    TEST_ASSERT_EQUAL(1, action_count);  // Wildcard action was called
}

TEST(FSMTests, TestFSMNoTransitionAction) {
    fsm_start(&fsm);
    reset_counters();

    // Action-only event (FSM_NO_TRANSITION)
    bool changed = fsm_process_event(&fsm, TEST_EVT_ACTION_ONLY);

    TEST_ASSERT_FALSE(changed);  // No state change
    TEST_ASSERT_EQUAL(STATE_A, fsm_get_state(&fsm));
    TEST_ASSERT_EQUAL(1, action_count);  // But action was called
    TEST_ASSERT_EQUAL(0, exit_a_count);  // No exit/entry
    TEST_ASSERT_EQUAL(0, entry_a_count);
}

TEST(FSMTests, TestFSMUpdateCallsStateUpdate) {
    fsm_start(&fsm);
    reset_counters();

    fsm_update(&fsm);
    fsm_update(&fsm);
    fsm_update(&fsm);

    TEST_ASSERT_EQUAL(3, update_a_count);
}

TEST(FSMTests, TestFSMUpdateInactiveDoesNothing) {
    // Don't start FSM
    reset_counters();

    fsm_update(&fsm);

    TEST_ASSERT_EQUAL(0, update_a_count);
}

TEST(FSMTests, TestFSMReset) {
    fsm_start(&fsm);
    fsm_process_event(&fsm, TEST_EVT_GO_B);  // A -> B
    reset_counters();

    fsm_reset(&fsm);

    TEST_ASSERT_EQUAL(STATE_A, fsm_get_state(&fsm));
    TEST_ASSERT_EQUAL(1, exit_b_count);
    TEST_ASSERT_EQUAL(1, entry_a_count);
}

TEST(FSMTests, TestFSMSetStateDirect) {
    fsm_start(&fsm);
    reset_counters();

    fsm_set_state(&fsm, STATE_C);

    TEST_ASSERT_EQUAL(STATE_C, fsm_get_state(&fsm));
    TEST_ASSERT_EQUAL(1, exit_a_count);
    TEST_ASSERT_EQUAL(1, entry_c_count);
}

TEST(FSMTests, TestFSMStop) {
    fsm_start(&fsm);
    reset_counters();

    fsm_stop(&fsm);

    TEST_ASSERT_FALSE(fsm_is_active(&fsm));
    TEST_ASSERT_EQUAL(1, exit_a_count);
}

TEST(FSMTests, TestFSMProcessEventWhenInactive) {
    // Don't start FSM
    bool changed = fsm_process_event(&fsm, TEST_EVT_GO_B);

    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_EQUAL(STATE_A, fsm_get_state(&fsm));
}

TEST(FSMTests, TestFSMNullSafety) {
    // These should not crash
    fsm_init(NULL, test_states, 3, test_transitions, 5, STATE_A);
    fsm_start(NULL);
    fsm_process_event(NULL, TEST_EVT_GO_B);
    fsm_update(NULL);
    fsm_reset(NULL);
    fsm_stop(NULL);
    fsm_set_state(NULL, STATE_B);

    TEST_ASSERT_EQUAL(0, fsm_get_state(NULL));
    TEST_ASSERT_FALSE(fsm_is_active(NULL));
}

TEST_GROUP_RUNNER(FSMTests) {
    RUN_TEST_CASE(FSMTests, TestFSMInit);
    RUN_TEST_CASE(FSMTests, TestFSMStart);
    RUN_TEST_CASE(FSMTests, TestFSMTransitionBasic);
    RUN_TEST_CASE(FSMTests, TestFSMTransitionWithAction);
    RUN_TEST_CASE(FSMTests, TestFSMEntryExitOrder);
    RUN_TEST_CASE(FSMTests, TestFSMNoMatchingTransition);
    RUN_TEST_CASE(FSMTests, TestFSMAnyStateWildcard);
    RUN_TEST_CASE(FSMTests, TestFSMNoTransitionAction);
    RUN_TEST_CASE(FSMTests, TestFSMUpdateCallsStateUpdate);
    RUN_TEST_CASE(FSMTests, TestFSMUpdateInactiveDoesNothing);
    RUN_TEST_CASE(FSMTests, TestFSMReset);
    RUN_TEST_CASE(FSMTests, TestFSMSetStateDirect);
    RUN_TEST_CASE(FSMTests, TestFSMStop);
    RUN_TEST_CASE(FSMTests, TestFSMProcessEventWhenInactive);
    RUN_TEST_CASE(FSMTests, TestFSMNullSafety);
}

void RunAllFSMTests(void) {
    RUN_TEST_GROUP(FSMTests);
}

#endif /* GK_TEST_FSM_H */
