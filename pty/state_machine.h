#ifndef STRANGE_STATE_MACHINE_H_
#define STRANGE_STATE_MACHINE_H_

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

enum strange_runtime_state {
  STRANGE_RUNTIME_STATE_PASSTHROUGH = 0,
  STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE,
  STRANGE_RUNTIME_STATE_SCREENSAVER_DISABLED,
  STRANGE_RUNTIME_STATE_SHUTTING_DOWN,
};

enum strange_runtime_event {
  STRANGE_RUNTIME_EVENT_USER_INPUT = 0,
  STRANGE_RUNTIME_EVENT_PTY_OUTPUT,
  STRANGE_RUNTIME_EVENT_TIMEOUT,
  STRANGE_RUNTIME_EVENT_RESIZE,
  STRANGE_RUNTIME_EVENT_DISABLE,
  STRANGE_RUNTIME_EVENT_SHUTDOWN,
};

struct strange_state_machine {
  enum strange_runtime_state state;
  int timeout_seconds;
  struct timespec last_activity_at;
};

struct strange_state_transition {
  enum strange_runtime_state previous_state;
  enum strange_runtime_state current_state;
  int state_changed;
  int entered_screensaver;
  int exited_screensaver;
  int recorded_activity;
};

void strange_state_machine_init(struct strange_state_machine *machine,
                                int timeout_seconds,
                                const struct timespec *start_time);
int strange_state_machine_timeout_due(
    const struct strange_state_machine *machine, const struct timespec *now);
struct strange_state_transition strange_state_machine_handle_event(
    struct strange_state_machine *machine, enum strange_runtime_event event,
    const struct timespec *now);

#ifdef __cplusplus
}
#endif

#endif
