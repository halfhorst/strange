#include "state_machine.h"

#include <stddef.h>

static int timespec_reached_timeout(const struct timespec *start,
                                    const struct timespec *now,
                                    int timeout_seconds) {
  if (now->tv_sec < start->tv_sec ||
      (now->tv_sec == start->tv_sec && now->tv_nsec < start->tv_nsec)) {
    return 0;
  }

  time_t seconds_elapsed = now->tv_sec - start->tv_sec;
  long nanoseconds_elapsed = now->tv_nsec - start->tv_nsec;

  if (nanoseconds_elapsed < 0) {
    seconds_elapsed -= 1;
    nanoseconds_elapsed += 1000000000L;
  }

  if (seconds_elapsed > timeout_seconds) {
    return 1;
  }
  if (seconds_elapsed < timeout_seconds) {
    return 0;
  }

  return nanoseconds_elapsed >= 0;
}

static struct strange_state_transition make_transition(
    enum strange_runtime_state previous_state,
    enum strange_runtime_state current_state, int recorded_activity) {
  struct strange_state_transition transition;

  transition.previous_state = previous_state;
  transition.current_state = current_state;
  transition.state_changed = previous_state != current_state;
  transition.entered_screensaver =
      previous_state != STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE &&
      current_state == STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE;
  transition.exited_screensaver =
      previous_state == STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE &&
      current_state != STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE;
  transition.recorded_activity = recorded_activity;

  return transition;
}

void strange_state_machine_init(struct strange_state_machine *machine,
                                int timeout_seconds,
                                const struct timespec *start_time) {
  machine->state = STRANGE_RUNTIME_STATE_PASSTHROUGH;
  machine->timeout_seconds = timeout_seconds;
  machine->last_activity_at = *start_time;
}

int strange_state_machine_timeout_due(
    const struct strange_state_machine *machine, const struct timespec *now) {
  if (machine->state != STRANGE_RUNTIME_STATE_PASSTHROUGH) {
    return 0;
  }

  return timespec_reached_timeout(&machine->last_activity_at, now,
                                  machine->timeout_seconds);
}

struct strange_state_transition strange_state_machine_handle_event(
    struct strange_state_machine *machine, enum strange_runtime_event event,
    const struct timespec *now) {
  enum strange_runtime_state previous_state = machine->state;
  int recorded_activity = 0;

  switch (event) {
  case STRANGE_RUNTIME_EVENT_USER_INPUT:
  case STRANGE_RUNTIME_EVENT_PTY_OUTPUT:
    machine->last_activity_at = *now;
    recorded_activity = 1;
    if (machine->state == STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE) {
      machine->state = STRANGE_RUNTIME_STATE_PASSTHROUGH;
    }
    break;
  case STRANGE_RUNTIME_EVENT_TIMEOUT:
    if (machine->state == STRANGE_RUNTIME_STATE_PASSTHROUGH) {
      machine->state = STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE;
    }
    break;
  case STRANGE_RUNTIME_EVENT_RESIZE:
    break;
  case STRANGE_RUNTIME_EVENT_DISABLE:
    if (machine->state != STRANGE_RUNTIME_STATE_SHUTTING_DOWN) {
      machine->state = STRANGE_RUNTIME_STATE_SCREENSAVER_DISABLED;
    }
    break;
  case STRANGE_RUNTIME_EVENT_SHUTDOWN:
    machine->state = STRANGE_RUNTIME_STATE_SHUTTING_DOWN;
    break;
  }

  return make_transition(previous_state, machine->state, recorded_activity);
}
