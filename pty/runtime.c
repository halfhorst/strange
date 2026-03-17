#include "runtime.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "pty.h"
#include "screensaver.h"
#include "state_machine.h"
#include "visible_screen.h"

#define STRANGE_DISABLE_KEY 0x11
#define STRANGE_POLL_INTERVAL_USEC 100000
#define STRANGE_BUFFER_SIZE (1024 * 1024)

static int monotonic_now(struct timespec *now) {
  if (clock_gettime(CLOCK_MONOTONIC, now) == -1) {
    perror("clock_gettime");
    return -1;
  }

  return 0;
}

static int write_all(int fd, const char *buffer, size_t length) {
  size_t written = 0;

  while (written < length) {
    ssize_t result = write(fd, buffer + written, length - written);
    if (result > 0) {
      written += (size_t)result;
      continue;
    }
    if (result == -1 && errno == EINTR) {
      continue;
    }
    if (result == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return 0;
    }
    return -1;
  }

  return 0;
}

static int apply_transition_effects(
    const struct strange_state_transition *transition,
    const struct timespec *now,
    struct strange_visible_screen *visible_screen) {
  if (transition->entered_screensaver) {
    if (strange_visible_screen_capture_snapshot(visible_screen) == -1 ||
        enter_screensaver() == -1) {
      perror("screensaver");
      return -1;
    }
  }
  if (transition->exited_screensaver) {
    leave_screensaver();
    if (strange_visible_screen_restore_to_fd(visible_screen, STDOUT_FILENO) ==
        -1) {
      perror("restore");
      return -1;
    }
  }
  if (transition->current_state == STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE &&
      render_screensaver_frame(now) == -1) {
    return -1;
  }

  return 0;
}

static int handle_runtime_event(struct strange_state_machine *machine,
                                enum strange_runtime_event event,
                                const struct timespec *now,
                                struct strange_visible_screen *visible_screen) {
  struct strange_state_transition transition =
      strange_state_machine_handle_event(machine, event, now);
  return apply_transition_effects(&transition, now, visible_screen);
}

static int forward_input_slice(struct strange_state_machine *machine,
                               const char *buffer, size_t length,
                               const struct timespec *now,
                               struct strange_visible_screen *visible_screen) {
  if (length == 0) {
    return 0;
  }

  if (handle_runtime_event(machine, STRANGE_RUNTIME_EVENT_USER_INPUT, now,
                           visible_screen) == -1) {
    return -1;
  }

  if (write_all(master_fd, buffer, length) == -1) {
    perror("write");
    return -1;
  }

  return 0;
}

static int handle_stdin_buffer(struct strange_state_machine *machine,
                               const char *buffer, size_t length,
                               const struct timespec *now,
                               struct strange_visible_screen *visible_screen) {
  if (machine->state == STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE) {
    enum strange_runtime_event event =
        memchr(buffer, STRANGE_DISABLE_KEY, length) != NULL
            ? STRANGE_RUNTIME_EVENT_DISABLE
            : STRANGE_RUNTIME_EVENT_USER_INPUT;
    return handle_runtime_event(machine, event, now, visible_screen);
  }

  size_t slice_start = 0;

  for (size_t index = 0; index < length; ++index) {
    if ((unsigned char)buffer[index] != STRANGE_DISABLE_KEY) {
      continue;
    }

    if (forward_input_slice(machine, buffer + slice_start, index - slice_start,
                            now, visible_screen) == -1) {
      return -1;
    }
    if (handle_runtime_event(machine, STRANGE_RUNTIME_EVENT_DISABLE, now,
                             visible_screen) == -1) {
      return -1;
    }

    slice_start = index + 1;
  }

  return forward_input_slice(machine, buffer + slice_start,
                             length - slice_start, now, visible_screen);
}

static int sync_resize_if_needed(enum strange_runtime_state state,
                                 const struct timespec *now,
                                 struct strange_visible_screen *visible_screen) {
  int w = 0;
  int h = 0;

  if (!strange_consume_resize_event()) {
    return 0;
  }
  if (strange_sync_pty_window_size() == -1) {
    perror("ioctl");
    return -1;
  }
  if (strange_get_terminal_size(STDOUT_FILENO, &w, &h) == -1 ||
      strange_visible_screen_resize(
          visible_screen, w, h,
          state == STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE,
          state == STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE) == -1) {
    perror("resize");
    return -1;
  }
  if (state == STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE &&
      render_screensaver_frame(now) == -1) {
    return -1;
  }

  return 0;
}

int strange_run(const struct strange_options *options) {
  int status = 1;
  int stdin_flags = -1;
  int runtime_status = 0;
  char buffer[STRANGE_BUFFER_SIZE];
  struct strange_visible_screen visible_screen = {0};
  struct strange_state_machine machine = {
      .state = STRANGE_RUNTIME_STATE_PASSTHROUGH,
      .timeout_seconds = options->timeout_seconds,
      .last_activity_at = {0, 0},
  };
  struct timespec now;
  int w = 0;
  int h = 0;

  if (options == NULL || options->screensaver_descriptor == NULL ||
      strange_set_screensaver_descriptor(options->screensaver_descriptor) ==
          -1) {
    fprintf(stderr, "Failed to configure screensaver\n");
    return 1;
  }

  if (setup_pty_and_shell() < 0) {
    fprintf(stderr, "Failed to set up PTY and shell\n");
    return 1;
  }
  if (strange_get_terminal_size(STDOUT_FILENO, &w, &h) == -1 ||
      strange_visible_screen_init(&visible_screen, w, h) == -1) {
    perror("terminal size");
    goto cleanup;
  }

  stdin_flags = fcntl(STDIN_FILENO, F_GETFL);
  if (stdin_flags == -1 ||
      fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK) == -1) {
    perror("fcntl");
    goto cleanup;
  }

  enable_raw_mode();

  if (monotonic_now(&now) == -1) {
    goto cleanup;
  }
  strange_state_machine_init(&machine, options->timeout_seconds, &now);

  while (machine.state != STRANGE_RUNTIME_STATE_SHUTTING_DOWN) {
    fd_set read_fds;
    int max_fd = STDIN_FILENO;

    if (monotonic_now(&now) == -1) {
      goto cleanup;
    }

    if (sync_resize_if_needed(machine.state, &now, &visible_screen) == -1) {
      goto cleanup;
    }

    if (strange_shutdown_requested()) {
      handle_runtime_event(&machine, STRANGE_RUNTIME_EVENT_SHUTDOWN, &now,
                           &visible_screen);
      break;
    }

    if (strange_state_machine_timeout_due(&machine, &now)) {
      if (handle_runtime_event(&machine, STRANGE_RUNTIME_EVENT_TIMEOUT, &now,
                               &visible_screen) == -1) {
        goto cleanup;
      }
    } else if (machine.state == STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE) {
      render_screensaver_frame(&now);
    }

    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    if (master_fd >= 0) {
      FD_SET(master_fd, &read_fds);
      if (master_fd > max_fd) {
        max_fd = master_fd;
      }
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = STRANGE_POLL_INTERVAL_USEC;

    int ready = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("select");
      goto cleanup;
    }

    if (ready > 0 && FD_ISSET(STDIN_FILENO, &read_fds)) {
      ssize_t bytes = read(STDIN_FILENO, buffer, sizeof(buffer));
      if (bytes > 0) {
        if (monotonic_now(&now) == -1) {
          goto cleanup;
        }
        if (handle_stdin_buffer(&machine, buffer, (size_t)bytes, &now,
                                &visible_screen) == -1) {
          goto cleanup;
        }
      } else if (bytes == 0) {
        handle_runtime_event(&machine, STRANGE_RUNTIME_EVENT_SHUTDOWN, &now,
                             &visible_screen);
      } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
        perror("read");
        goto cleanup;
      }
    }

    if (ready > 0 && master_fd >= 0 && FD_ISSET(master_fd, &read_fds)) {
      ssize_t bytes = read(master_fd, buffer, sizeof(buffer));
      if (bytes > 0) {
        if (monotonic_now(&now) == -1) {
          goto cleanup;
        }
        if (handle_runtime_event(&machine, STRANGE_RUNTIME_EVENT_PTY_OUTPUT,
                                 &now, &visible_screen) == -1) {
          goto cleanup;
        }

        if (write_all(STDOUT_FILENO, buffer, (size_t)bytes) == -1) {
          perror("write");
          goto cleanup;
        }
        if (strange_visible_screen_write(&visible_screen, buffer,
                                         (size_t)bytes) == -1) {
          goto cleanup;
        }
      } else if (bytes == 0 || (bytes < 0 && errno == EIO)) {
        handle_runtime_event(&machine, STRANGE_RUNTIME_EVENT_SHUTDOWN, &now,
                             &visible_screen);
      } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
        perror("read");
        goto cleanup;
      }
    }

    int shell_status = 0;
    int shell_exited = poll_shell_exit(&shell_status);
    if (shell_exited == -1) {
      perror("waitpid");
      goto cleanup;
    }
    if (shell_exited == 1) {
      handle_runtime_event(&machine, STRANGE_RUNTIME_EVENT_SHUTDOWN, &now,
                           &visible_screen);
      runtime_status = 0;
      break;
    }
  }

  status = runtime_status;

cleanup:
  if (machine.state == STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE) {
    leave_screensaver();
  }
  strange_visible_screen_destroy(&visible_screen);
  disable_raw_mode();

  if (stdin_flags != -1 && fcntl(STDIN_FILENO, F_SETFL, stdin_flags) == -1) {
    perror("fcntl");
    status = 1;
  }

  cleanup_pty();

  if (status != 0) {
    return 1;
  }

  int shell_status = 0;
  int shell_exited = poll_shell_exit(&shell_status);
  if (shell_exited == -1) {
    perror("waitpid");
    return 1;
  }
  if (shell_exited == 1) {
    if (WIFEXITED(shell_status)) {
      return WEXITSTATUS(shell_status);
    }
    if (WIFSIGNALED(shell_status)) {
      return 128 + WTERMSIG(shell_status);
    }
  }

  return 0;
}
