#include "runtime.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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
#define STRANGE_FRAME_INTERVAL_USEC 16666
#define STRANGE_BUFFER_SIZE (64 * 1024)
#define STRANGE_HELD_OUTPUT_LIMIT (16 * 1024 * 1024)

static struct {
  char bytes[STRANGE_BUFFER_SIZE];
  size_t length;
} pending_input;

// Shell output that arrived while the screensaver was showing.
static struct {
  char *bytes;
  size_t length;
  size_t capacity;
} held_output;

static int monotonic_now(struct timespec *now) {
  if (clock_gettime(CLOCK_MONOTONIC, now) == -1) {
    perror("clock_gettime");
    return -1;
  }

  return 0;
}

static long frame_wait_usec(const struct timespec *frame_started) {
  struct timespec now;
  long elapsed_usec = 0;

  if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
    return STRANGE_FRAME_INTERVAL_USEC;
  }

  elapsed_usec = (long)(now.tv_sec - frame_started->tv_sec) * 1000000L +
                 (now.tv_nsec - frame_started->tv_nsec) / 1000L;
  if (elapsed_usec >= STRANGE_FRAME_INTERVAL_USEC) {
    return 0;
  }

  return STRANGE_FRAME_INTERVAL_USEC - elapsed_usec;
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
    return -1;
  }

  return 0;
}

static int flush_pending_input(void) {
  size_t written = 0;

  while (written < pending_input.length) {
    ssize_t result = write(master_fd, pending_input.bytes + written,
                           pending_input.length - written);
    if (result > 0) {
      written += (size_t)result;
      continue;
    }
    if (result == -1 && errno == EINTR) {
      continue;
    }
    if (result == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      break;
    }
    return -1;
  }

  pending_input.length -= written;
  memmove(pending_input.bytes, pending_input.bytes + written,
          pending_input.length);
  return 0;
}

static int forward_output(struct strange_visible_screen *visible_screen,
                          const char *buffer, size_t length) {
  if (write_all(STDOUT_FILENO, buffer, length) == -1) {
    perror("write");
    return -1;
  }

  return strange_visible_screen_write(visible_screen, buffer, length);
}

static int hold_output(const char *buffer, size_t length) {
  size_t capacity = held_output.capacity;
  char *bytes = NULL;

  if (length > STRANGE_HELD_OUTPUT_LIMIT - held_output.length) {
    return -1;
  }

  if (capacity == 0) {
    capacity = STRANGE_BUFFER_SIZE;
  }
  while (capacity - held_output.length < length) {
    capacity *= 2;
  }
  if (capacity != held_output.capacity) {
    bytes = realloc(held_output.bytes, capacity);
    if (bytes == NULL) {
      return -1;
    }
    held_output.bytes = bytes;
    held_output.capacity = capacity;
  }

  memcpy(held_output.bytes + held_output.length, buffer, length);
  held_output.length += length;
  return 0;
}

static int release_held_output(struct strange_visible_screen *visible_screen) {
  int result = 0;

  if (held_output.length > 0) {
    result = forward_output(visible_screen, held_output.bytes,
                            held_output.length);
  }

  free(held_output.bytes);
  memset(&held_output, 0, sizeof(held_output));
  return result;
}

static int exit_screensaver(struct strange_visible_screen *visible_screen) {
  leave_screensaver();
  if (strange_visible_screen_restore_to_fd(visible_screen, STDOUT_FILENO) ==
      -1) {
    perror("restore");
    return -1;
  }

  return release_held_output(visible_screen);
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
  if (transition->exited_screensaver &&
      exit_screensaver(visible_screen) == -1) {
    return -1;
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

  if (length > sizeof(pending_input.bytes) - pending_input.length) {
    length = sizeof(pending_input.bytes) - pending_input.length;
  }
  memcpy(pending_input.bytes + pending_input.length, buffer, length);
  pending_input.length += length;

  if (flush_pending_input() == -1) {
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

  pending_input.length = 0;
  memset(&held_output, 0, sizeof(held_output));
  enable_raw_mode();

  if (monotonic_now(&now) == -1) {
    goto cleanup;
  }
  strange_state_machine_init(&machine, options->timeout_seconds, &now);

  while (machine.state != STRANGE_RUNTIME_STATE_SHUTTING_DOWN) {
    fd_set read_fds;
    fd_set write_fds;
    int max_fd = STDIN_FILENO;
    size_t input_capacity = 0;

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

    input_capacity = sizeof(pending_input.bytes) - pending_input.length;

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    if (input_capacity > 0) {
      FD_SET(STDIN_FILENO, &read_fds);
    }
    if (master_fd >= 0) {
      FD_SET(master_fd, &read_fds);
      if (pending_input.length > 0) {
        FD_SET(master_fd, &write_fds);
      }
      if (master_fd > max_fd) {
        max_fd = master_fd;
      }
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec =
        machine.state == STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE
            ? frame_wait_usec(&now)
            : STRANGE_POLL_INTERVAL_USEC;

    int ready = select(max_fd + 1, &read_fds, &write_fds, NULL, &timeout);
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("select");
      goto cleanup;
    }

    if (ready > 0 && master_fd >= 0 && FD_ISSET(master_fd, &write_fds) &&
        flush_pending_input() == -1) {
      perror("write");
      goto cleanup;
    }

    if (ready > 0 && FD_ISSET(STDIN_FILENO, &read_fds)) {
      ssize_t bytes = read(STDIN_FILENO, buffer, input_capacity);
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
        if (machine.state == STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE &&
            hold_output(buffer, (size_t)bytes) == 0) {
          continue;
        }
        if (machine.state == STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE &&
            handle_runtime_event(&machine,
                                 STRANGE_RUNTIME_EVENT_OUTPUT_OVERFLOW, &now,
                                 &visible_screen) == -1) {
          goto cleanup;
        }
        if (forward_output(&visible_screen, buffer, (size_t)bytes) == -1) {
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
    exit_screensaver(&visible_screen);
  }
  strange_visible_screen_destroy(&visible_screen);
  disable_raw_mode();
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
