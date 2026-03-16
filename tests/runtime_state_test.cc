#include <time.h>

#include <gtest/gtest.h>

extern "C" {
#include "pty/state_machine.h"
}

namespace {

timespec Seconds(time_t seconds) {
  timespec value;
  value.tv_sec = seconds;
  value.tv_nsec = 0;
  return value;
}

TEST(RuntimeStateMachineTest, StartsInPassthroughAndMonitorsImmediately) {
  strange_state_machine machine;
  const timespec start = Seconds(100);
  const timespec before_timeout = Seconds(129);
  const timespec at_timeout = Seconds(130);

  strange_state_machine_init(&machine, 30, &start);

  EXPECT_EQ(machine.state, STRANGE_RUNTIME_STATE_PASSTHROUGH);
  EXPECT_FALSE(strange_state_machine_timeout_due(&machine, &before_timeout));
  EXPECT_TRUE(strange_state_machine_timeout_due(&machine, &at_timeout));
}

TEST(RuntimeStateMachineTest, UserInputResetsInactivityTimer) {
  strange_state_machine machine;
  const timespec start = Seconds(10);
  const timespec input_time = Seconds(25);
  const timespec before_timeout = Seconds(54);
  const timespec at_timeout = Seconds(55);

  strange_state_machine_init(&machine, 30, &start);

  const strange_state_transition transition = strange_state_machine_handle_event(
      &machine, STRANGE_RUNTIME_EVENT_USER_INPUT, &input_time);

  EXPECT_EQ(transition.previous_state, STRANGE_RUNTIME_STATE_PASSTHROUGH);
  EXPECT_EQ(transition.current_state, STRANGE_RUNTIME_STATE_PASSTHROUGH);
  EXPECT_TRUE(transition.recorded_activity);
  EXPECT_EQ(machine.last_activity_at.tv_sec, input_time.tv_sec);
  EXPECT_FALSE(strange_state_machine_timeout_due(&machine, &before_timeout));
  EXPECT_TRUE(strange_state_machine_timeout_due(&machine, &at_timeout));
}

TEST(RuntimeStateMachineTest, PtyOutputResetsInactivityTimer) {
  strange_state_machine machine;
  const timespec start = Seconds(40);
  const timespec output_time = Seconds(58);
  const timespec before_timeout = Seconds(67);
  const timespec at_timeout = Seconds(68);

  strange_state_machine_init(&machine, 10, &start);

  const strange_state_transition transition = strange_state_machine_handle_event(
      &machine, STRANGE_RUNTIME_EVENT_PTY_OUTPUT, &output_time);

  EXPECT_EQ(transition.current_state, STRANGE_RUNTIME_STATE_PASSTHROUGH);
  EXPECT_TRUE(transition.recorded_activity);
  EXPECT_EQ(machine.last_activity_at.tv_sec, output_time.tv_sec);
  EXPECT_FALSE(strange_state_machine_timeout_due(&machine, &before_timeout));
  EXPECT_TRUE(strange_state_machine_timeout_due(&machine, &at_timeout));
}

TEST(RuntimeStateMachineTest, TimeoutTransitionsPassthroughToScreensaver) {
  strange_state_machine machine;
  const timespec start = Seconds(0);
  const timespec timeout_time = Seconds(5);

  strange_state_machine_init(&machine, 5, &start);

  const strange_state_transition transition = strange_state_machine_handle_event(
      &machine, STRANGE_RUNTIME_EVENT_TIMEOUT, &timeout_time);

  EXPECT_EQ(transition.previous_state, STRANGE_RUNTIME_STATE_PASSTHROUGH);
  EXPECT_EQ(transition.current_state,
            STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE);
  EXPECT_TRUE(transition.entered_screensaver);
}

TEST(RuntimeStateMachineTest, ResizeDoesNotResetInactivity) {
  strange_state_machine machine;
  const timespec start = Seconds(100);
  const timespec resize_time = Seconds(129);
  const timespec before_timeout = Seconds(129);
  const timespec at_timeout = Seconds(130);

  strange_state_machine_init(&machine, 30, &start);

  const strange_state_transition transition = strange_state_machine_handle_event(
      &machine, STRANGE_RUNTIME_EVENT_RESIZE, &resize_time);

  EXPECT_EQ(transition.current_state, STRANGE_RUNTIME_STATE_PASSTHROUGH);
  EXPECT_FALSE(transition.recorded_activity);
  EXPECT_FALSE(strange_state_machine_timeout_due(&machine, &before_timeout));
  EXPECT_TRUE(strange_state_machine_timeout_due(&machine, &at_timeout));
}

TEST(RuntimeStateMachineTest, ActivityLeavesScreensaverState) {
  strange_state_machine machine;
  const timespec start = Seconds(0);
  const timespec timeout_time = Seconds(3);
  const timespec wake_time = Seconds(4);

  strange_state_machine_init(&machine, 3, &start);
  strange_state_machine_handle_event(&machine, STRANGE_RUNTIME_EVENT_TIMEOUT,
                                     &timeout_time);

  const strange_state_transition transition = strange_state_machine_handle_event(
      &machine, STRANGE_RUNTIME_EVENT_PTY_OUTPUT, &wake_time);

  EXPECT_EQ(transition.previous_state,
            STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE);
  EXPECT_EQ(transition.current_state, STRANGE_RUNTIME_STATE_PASSTHROUGH);
  EXPECT_TRUE(transition.exited_screensaver);
  EXPECT_EQ(machine.last_activity_at.tv_sec, wake_time.tv_sec);
}

TEST(RuntimeStateMachineTest,
     DisableFromScreensaverLeavesScreensaverAndPreventsFutureTimeouts) {
  strange_state_machine machine;
  const timespec start = Seconds(0);
  const timespec timeout_time = Seconds(3);
  const timespec disable_time = Seconds(4);
  const timespec long_after_disable = Seconds(100);

  strange_state_machine_init(&machine, 3, &start);
  strange_state_machine_handle_event(&machine, STRANGE_RUNTIME_EVENT_TIMEOUT,
                                     &timeout_time);

  const strange_state_transition transition = strange_state_machine_handle_event(
      &machine, STRANGE_RUNTIME_EVENT_DISABLE, &disable_time);

  EXPECT_EQ(transition.previous_state,
            STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE);
  EXPECT_EQ(transition.current_state,
            STRANGE_RUNTIME_STATE_SCREENSAVER_DISABLED);
  EXPECT_TRUE(transition.exited_screensaver);
  EXPECT_FALSE(strange_state_machine_timeout_due(&machine, &long_after_disable));
}

TEST(RuntimeStateMachineTest,
     DisableStateIgnoresFurtherActivityAndStaysDisabled) {
  strange_state_machine machine;
  const timespec start = Seconds(10);
  const timespec disable_time = Seconds(15);
  const timespec input_time = Seconds(16);
  const timespec output_time = Seconds(17);

  strange_state_machine_init(&machine, 30, &start);
  strange_state_machine_handle_event(&machine, STRANGE_RUNTIME_EVENT_DISABLE,
                                     &disable_time);

  const strange_state_transition input_transition =
      strange_state_machine_handle_event(&machine,
                                         STRANGE_RUNTIME_EVENT_USER_INPUT,
                                         &input_time);
  EXPECT_EQ(input_transition.current_state,
            STRANGE_RUNTIME_STATE_SCREENSAVER_DISABLED);
  EXPECT_TRUE(input_transition.recorded_activity);

  const strange_state_transition output_transition =
      strange_state_machine_handle_event(&machine,
                                         STRANGE_RUNTIME_EVENT_PTY_OUTPUT,
                                         &output_time);
  EXPECT_EQ(output_transition.current_state,
            STRANGE_RUNTIME_STATE_SCREENSAVER_DISABLED);
  EXPECT_TRUE(output_transition.recorded_activity);
}

TEST(RuntimeStateMachineTest, DisableAndShutdownStatesRemainExplicit) {
  strange_state_machine machine;
  const timespec start = Seconds(5);
  const timespec disable_time = Seconds(9);
  const timespec shutdown_time = Seconds(10);
  const timespec long_after_disable = Seconds(100);

  strange_state_machine_init(&machine, 30, &start);

  strange_state_transition disable_transition =
      strange_state_machine_handle_event(
          &machine, STRANGE_RUNTIME_EVENT_DISABLE, &disable_time);
  EXPECT_EQ(disable_transition.current_state,
            STRANGE_RUNTIME_STATE_SCREENSAVER_DISABLED);
  EXPECT_FALSE(strange_state_machine_timeout_due(&machine, &long_after_disable));

  strange_state_transition shutdown_transition =
      strange_state_machine_handle_event(
          &machine, STRANGE_RUNTIME_EVENT_SHUTDOWN, &shutdown_time);
  EXPECT_EQ(shutdown_transition.current_state,
            STRANGE_RUNTIME_STATE_SHUTTING_DOWN);
}

}  // namespace
