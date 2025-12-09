#ifndef GK_TEST_EVENTS_H
#define GK_TEST_EVENTS_H

#include "unity.h"
#include "unity_fixture.h"
#include "events/events.h"

static EventProcessor ep;
static EventInput input;

TEST_GROUP(EventProcessorTests);

TEST_SETUP(EventProcessorTests) {
    event_processor_init(&ep);
    input.button_a = false;
    input.button_b = false;
    input.cv_in = false;
    input.current_time = 0;
}

TEST_TEAR_DOWN(EventProcessorTests) {
}

TEST(EventProcessorTests, TestEventProcessorInit) {
    TEST_ASSERT_FALSE(event_processor_a_pressed(&ep));
    TEST_ASSERT_FALSE(event_processor_b_pressed(&ep));
    TEST_ASSERT_FALSE(event_processor_a_holding(&ep));
    TEST_ASSERT_FALSE(event_processor_b_holding(&ep));
}

TEST(EventProcessorTests, TestEventAPress) {
    input.button_a = true;
    input.current_time = 100;

    Event evt = event_processor_update(&ep, &input);

    TEST_ASSERT_EQUAL(EVT_A_PRESS, evt);
    TEST_ASSERT_TRUE(event_processor_a_pressed(&ep));
}

TEST(EventProcessorTests, TestEventATap) {
    // Press
    input.button_a = true;
    input.current_time = 100;
    event_processor_update(&ep, &input);

    // Release quickly (before hold threshold)
    input.button_a = false;
    input.current_time = 200;  // 100ms later, well under 500ms hold threshold

    Event evt = event_processor_update(&ep, &input);

    TEST_ASSERT_EQUAL(EVT_A_TAP, evt);
}

TEST(EventProcessorTests, TestEventAHold) {
    // Press
    input.button_a = true;
    input.current_time = 100;
    event_processor_update(&ep, &input);

    // Hold past threshold
    input.current_time = 100 + EP_HOLD_THRESHOLD_MS;

    Event evt = event_processor_update(&ep, &input);

    TEST_ASSERT_EQUAL(EVT_A_HOLD, evt);
    TEST_ASSERT_TRUE(event_processor_a_holding(&ep));
}

TEST(EventProcessorTests, TestEventAReleaseAfterHold) {
    // Press
    input.button_a = true;
    input.current_time = 100;
    event_processor_update(&ep, &input);

    // Hold past threshold
    input.current_time = 100 + EP_HOLD_THRESHOLD_MS;
    event_processor_update(&ep, &input);

    // Release
    input.button_a = false;
    input.current_time = 100 + EP_HOLD_THRESHOLD_MS + 100;

    Event evt = event_processor_update(&ep, &input);

    TEST_ASSERT_EQUAL(EVT_A_RELEASE, evt);  // Release, not tap
}

TEST(EventProcessorTests, TestEventBPress) {
    input.button_b = true;
    input.current_time = 100;

    Event evt = event_processor_update(&ep, &input);

    TEST_ASSERT_EQUAL(EVT_B_PRESS, evt);
    TEST_ASSERT_TRUE(event_processor_b_pressed(&ep));
}

TEST(EventProcessorTests, TestEventBTap) {
    // Press
    input.button_b = true;
    input.current_time = 100;
    event_processor_update(&ep, &input);

    // Release quickly
    input.button_b = false;
    input.current_time = 200;

    Event evt = event_processor_update(&ep, &input);

    TEST_ASSERT_EQUAL(EVT_B_TAP, evt);
}

TEST(EventProcessorTests, TestEventBHold) {
    // Press
    input.button_b = true;
    input.current_time = 100;
    event_processor_update(&ep, &input);

    // Hold past threshold
    input.current_time = 100 + EP_HOLD_THRESHOLD_MS;

    Event evt = event_processor_update(&ep, &input);

    TEST_ASSERT_EQUAL(EVT_B_HOLD, evt);
    TEST_ASSERT_TRUE(event_processor_b_holding(&ep));
}

TEST(EventProcessorTests, TestEventCVRise) {
    input.cv_in = true;
    input.current_time = 100;

    Event evt = event_processor_update(&ep, &input);

    TEST_ASSERT_EQUAL(EVT_CV_RISE, evt);
}

TEST(EventProcessorTests, TestEventCVFall) {
    // First go high
    input.cv_in = true;
    input.current_time = 100;
    event_processor_update(&ep, &input);

    // Then go low
    input.cv_in = false;
    input.current_time = 200;

    Event evt = event_processor_update(&ep, &input);

    TEST_ASSERT_EQUAL(EVT_CV_FALL, evt);
}

TEST(EventProcessorTests, TestEventMenuEnter) {
    // Menu Enter: A pressed first, then B held for threshold
    // Press A first
    input.button_a = true;
    input.current_time = 100;
    event_processor_update(&ep, &input);

    // Press B while A is still held (A was pressed first)
    input.button_b = true;
    input.current_time = 200;
    event_processor_update(&ep, &input);

    // A reaches hold threshold first (A was pressed 100ms earlier)
    input.current_time = 100 + EP_HOLD_THRESHOLD_MS;
    Event evt_a = event_processor_update(&ep, &input);
    TEST_ASSERT_EQUAL(EVT_A_HOLD, evt_a);

    // B reaches hold threshold - this triggers menu enter
    // (because A was pressed before B)
    input.current_time = 200 + EP_HOLD_THRESHOLD_MS;
    Event evt = event_processor_update(&ep, &input);

    TEST_ASSERT_EQUAL(EVT_MENU_ENTER, evt);
}

TEST(EventProcessorTests, TestEventModeChange) {
    // Mode Change: B pressed first, then A held for threshold
    // Press B first
    input.button_b = true;
    input.current_time = 100;
    event_processor_update(&ep, &input);

    // Press A while B is still held (B was pressed first)
    input.button_a = true;
    input.current_time = 200;
    event_processor_update(&ep, &input);

    // B reaches hold threshold first (B was pressed 100ms earlier)
    input.current_time = 100 + EP_HOLD_THRESHOLD_MS;
    Event evt_b = event_processor_update(&ep, &input);
    TEST_ASSERT_EQUAL(EVT_B_HOLD, evt_b);

    // A reaches hold threshold - this triggers mode change
    // (because B was pressed before A)
    input.current_time = 200 + EP_HOLD_THRESHOLD_MS;
    Event evt = event_processor_update(&ep, &input);

    TEST_ASSERT_EQUAL(EVT_MODE_CHANGE, evt);
}

TEST(EventProcessorTests, TestNoDoubleFirePress) {
    // Press A
    input.button_a = true;
    input.current_time = 100;
    Event evt1 = event_processor_update(&ep, &input);
    TEST_ASSERT_EQUAL(EVT_A_PRESS, evt1);

    // Keep holding, should not fire again
    input.current_time = 150;
    Event evt2 = event_processor_update(&ep, &input);

    // Should be NONE until hold threshold
    TEST_ASSERT_EQUAL(EVT_NONE, evt2);
}

TEST(EventProcessorTests, TestNoDoubleFireHold) {
    // Press A
    input.button_a = true;
    input.current_time = 100;
    event_processor_update(&ep, &input);

    // Hold past threshold - first hold event
    input.current_time = 100 + EP_HOLD_THRESHOLD_MS;
    Event evt1 = event_processor_update(&ep, &input);
    TEST_ASSERT_EQUAL(EVT_A_HOLD, evt1);

    // Keep holding - should not fire hold again
    input.current_time = 100 + EP_HOLD_THRESHOLD_MS + 100;
    Event evt2 = event_processor_update(&ep, &input);

    TEST_ASSERT_EQUAL(EVT_NONE, evt2);
}

TEST(EventProcessorTests, TestAPressHasPriorityOverBPress) {
    // Press both simultaneously
    input.button_a = true;
    input.button_b = true;
    input.current_time = 100;

    Event evt = event_processor_update(&ep, &input);

    // A press should have priority
    TEST_ASSERT_EQUAL(EVT_A_PRESS, evt);
}

TEST(EventProcessorTests, TestButtonPressHasPriorityOverCV) {
    // Button press and CV rise at same time
    input.button_a = true;
    input.cv_in = true;
    input.current_time = 100;

    Event evt = event_processor_update(&ep, &input);

    // Button press should have priority
    TEST_ASSERT_EQUAL(EVT_A_PRESS, evt);
}

TEST(EventProcessorTests, TestEventProcessorReset) {
    // Get into some state
    input.button_a = true;
    input.current_time = 100;
    event_processor_update(&ep, &input);
    input.current_time = 100 + EP_HOLD_THRESHOLD_MS;
    event_processor_update(&ep, &input);

    TEST_ASSERT_TRUE(event_processor_a_holding(&ep));

    // Reset
    event_processor_reset(&ep);

    TEST_ASSERT_FALSE(event_processor_a_pressed(&ep));
    TEST_ASSERT_FALSE(event_processor_a_holding(&ep));
}

TEST(EventProcessorTests, TestNullSafety) {
    // These should not crash
    event_processor_init(NULL);
    event_processor_reset(NULL);

    Event evt = event_processor_update(NULL, &input);
    TEST_ASSERT_EQUAL(EVT_NONE, evt);

    evt = event_processor_update(&ep, NULL);
    TEST_ASSERT_EQUAL(EVT_NONE, evt);

    TEST_ASSERT_FALSE(event_processor_a_pressed(NULL));
    TEST_ASSERT_FALSE(event_processor_b_pressed(NULL));
    TEST_ASSERT_FALSE(event_processor_a_holding(NULL));
    TEST_ASSERT_FALSE(event_processor_b_holding(NULL));
}

TEST_GROUP_RUNNER(EventProcessorTests) {
    RUN_TEST_CASE(EventProcessorTests, TestEventProcessorInit);
    RUN_TEST_CASE(EventProcessorTests, TestEventAPress);
    RUN_TEST_CASE(EventProcessorTests, TestEventATap);
    RUN_TEST_CASE(EventProcessorTests, TestEventAHold);
    RUN_TEST_CASE(EventProcessorTests, TestEventAReleaseAfterHold);
    RUN_TEST_CASE(EventProcessorTests, TestEventBPress);
    RUN_TEST_CASE(EventProcessorTests, TestEventBTap);
    RUN_TEST_CASE(EventProcessorTests, TestEventBHold);
    RUN_TEST_CASE(EventProcessorTests, TestEventCVRise);
    RUN_TEST_CASE(EventProcessorTests, TestEventCVFall);
    RUN_TEST_CASE(EventProcessorTests, TestEventMenuEnter);
    RUN_TEST_CASE(EventProcessorTests, TestEventModeChange);
    RUN_TEST_CASE(EventProcessorTests, TestNoDoubleFirePress);
    RUN_TEST_CASE(EventProcessorTests, TestNoDoubleFireHold);
    RUN_TEST_CASE(EventProcessorTests, TestAPressHasPriorityOverBPress);
    RUN_TEST_CASE(EventProcessorTests, TestButtonPressHasPriorityOverCV);
    RUN_TEST_CASE(EventProcessorTests, TestEventProcessorReset);
    RUN_TEST_CASE(EventProcessorTests, TestNullSafety);
}

void RunAllEventProcessorTests(void) {
    RUN_TEST_GROUP(EventProcessorTests);
}

#endif /* GK_TEST_EVENTS_H */
