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
#include "src/session.h"
#include "state_machine.h"
#include "terminal_modes.h"

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

// Set while the screensaver is drawn over a full-screen program's own screen.
// The program is told a wrong height meanwhile so that it repaints on wake.
static int drew_over_child_screen;

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

static int forward_output(struct strange_terminal_modes *modes,
                          const char *buffer, size_t length) {
  if (write_all(STDOUT_FILENO, buffer, length) == -1) {
    perror("write");
    return -1;
  }

  strange_terminal_modes_write(modes, buffer, length);
  return 0;
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

static int release_held_output(struct strange_terminal_modes *modes) {
  int result = 0;

  if (held_output.length > 0) {
    result = forward_output(modes, held_output.bytes,
                            held_output.length);
  }

  free(held_output.bytes);
  memset(&held_output, 0, sizeof(held_output));
  return result;
}

static int exit_screensaver(struct strange_terminal_modes *modes) {
  leave_screensaver(modes->cursor_visible);
  if (release_held_output(modes) == -1) {
    return -1;
  }

  if (drew_over_child_screen) {
    drew_over_child_screen = 0;
    if (strange_sync_pty_window_size(0) == -1) {
      perror("ioctl");
      return -1;
    }
  }

  return 0;
}

// A screensaver that fails is turned off for the session rather than taking
// the shell down with it.
static int disable_failed_screensaver(struct strange_state_machine *machine,
                                      const struct timespec *now,
                                      struct strange_terminal_modes *modes) {
  struct strange_state_transition transition =
      strange_state_machine_handle_event(machine,
                                         STRANGE_RUNTIME_EVENT_DISABLE, now);

  if (transition.exited_screensaver && exit_screensaver(modes) == -1) {
    return -1;
  }

  fprintf(stderr, "\r\nstrange: screensaver disabled: %s\r\n",
          strange_screensaver_error());
  return 0;
}

static int render_frame(struct strange_state_machine *machine,
                        const struct timespec *now,
                        struct strange_terminal_modes *modes) {
  if (render_screensaver_frame(now) == 0) {
    return 0;
  }

  return disable_failed_screensaver(machine, now, modes);
}

static int handle_runtime_event(struct strange_state_machine *machine,
                                enum strange_runtime_event event,
                                const struct timespec *now,
                                struct strange_terminal_modes *modes) {
  struct strange_state_transition transition =
      strange_state_machine_handle_event(machine, event, now);

  if (transition.entered_screensaver) {
    drew_over_child_screen = modes->alternate_screen;
    if (enter_screensaver(!drew_over_child_screen) == -1) {
      return disable_failed_screensaver(machine, now, modes);
    }
    if (drew_over_child_screen && strange_sync_pty_window_size(1) == -1) {
      perror("ioctl");
      return -1;
    }
  }
  if (transition.exited_screensaver && exit_screensaver(modes) == -1) {
    return -1;
  }
  if (transition.current_state == STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE) {
    return render_frame(machine, now, modes);
  }

  return 0;
}

static int forward_input_slice(struct strange_state_machine *machine,
                               const char *buffer, size_t length,
                               const struct timespec *now,
                               struct strange_terminal_modes *modes) {
  if (length == 0) {
    return 0;
  }

  if (handle_runtime_event(machine, STRANGE_RUNTIME_EVENT_USER_INPUT, now,
                           modes) == -1) {
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
                               struct strange_terminal_modes *modes) {
  if (machine->state == STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE) {
    enum strange_runtime_event event =
        memchr(buffer, STRANGE_DISABLE_KEY, length) != NULL
            ? STRANGE_RUNTIME_EVENT_DISABLE
            : STRANGE_RUNTIME_EVENT_USER_INPUT;
    return handle_runtime_event(machine, event, now, modes);
  }

  size_t slice_start = 0;

  for (size_t index = 0; index < length; ++index) {
    if ((unsigned char)buffer[index] != STRANGE_DISABLE_KEY) {
      continue;
    }

    if (forward_input_slice(machine, buffer + slice_start, index - slice_start,
                            now, modes) == -1) {
      return -1;
    }
    if (handle_runtime_event(machine, STRANGE_RUNTIME_EVENT_DISABLE, now,
                             modes) == -1) {
      return -1;
    }

    slice_start = index + 1;
  }

  return forward_input_slice(machine, buffer + slice_start,
                             length - slice_start, now, modes);
}

static int sync_resize_if_needed(struct strange_state_machine *machine,
                                 const struct timespec *now,
                                 struct strange_terminal_modes *modes) {
  if (!strange_consume_resize_event()) {
    return 0;
  }
  if (strange_sync_pty_window_size(drew_over_child_screen) == -1) {
    perror("ioctl");
    return -1;
  }
  if (machine->state == STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE) {
    return render_frame(machine, now, modes);
  }

  return 0;
}

int strange_preview(const struct strange_screensaver_descriptor *descriptor) {
  struct timespec now;
  char keys[64];
  int failed = 0;

  if (strange_set_screensaver_descriptor(descriptor) == -1 ||
      strange_install_signal_handlers() == -1) {
    fprintf(stderr, "Failed to configure screensaver\n");
    return 1;
  }

  strange_set_screensaver_preview(1);
  enable_raw_mode();
  failed = enter_screensaver(1) == -1;

  while (!failed && !strange_shutdown_requested()) {
    fd_set read_fds;
    struct timeval timeout = {0, 0};
    int ready = 0;

    if (monotonic_now(&now) == -1 || render_screensaver_frame(&now) == -1) {
      failed = 1;
      break;
    }

    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    timeout.tv_usec = frame_wait_usec(&now);
    ready = select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &timeout);
    if (ready > 0) {
      (void)read(STDIN_FILENO, keys, sizeof(keys));
      break;
    }
    if (ready < 0 && errno != EINTR) {
      failed = 1;
    }
  }

  leave_screensaver(1);
  disable_raw_mode();
  strange_set_screensaver_preview(0);

  if (failed) {
    fprintf(stderr, "strange: %s\n", strange_screensaver_error());
  }
  return failed;
}

int strange_run(const struct strange_options *options) {
  int status = 1;
  int runtime_status = 0;
  char buffer[STRANGE_BUFFER_SIZE];
  struct strange_terminal_modes modes;
  struct strange_state_machine machine = {
      .state = STRANGE_RUNTIME_STATE_PASSTHROUGH,
      .timeout_seconds = options->timeout_seconds,
      .last_activity_at = {0, 0},
  };
  struct timespec now;

  if (options == NULL || options->screensaver_descriptor == NULL ||
      strange_set_screensaver_descriptor(options->screensaver_descriptor) ==
          -1) {
    fprintf(stderr, "Failed to configure screensaver\n");
    return 1;
  }

  if (strange_session_export(options->screensaver_descriptor->name,
                             options->timeout_seconds,
                             options->cover_fullscreen) == -1) {
    perror("setenv");
    return 1;
  }
  if (setup_pty_and_shell() < 0) {
    fprintf(stderr, "Failed to set up PTY and shell\n");
    return 1;
  }
  strange_terminal_modes_init(&modes);
  pending_input.length = 0;
  drew_over_child_screen = 0;
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

    if (sync_resize_if_needed(&machine, &now, &modes) == -1) {
      goto cleanup;
    }

    if (strange_shutdown_requested()) {
      handle_runtime_event(&machine, STRANGE_RUNTIME_EVENT_SHUTDOWN, &now,
                           &modes);
      break;
    }

    if (!options->cover_fullscreen && modes.alternate_screen &&
        machine.state == STRANGE_RUNTIME_STATE_PASSTHROUGH) {
      handle_runtime_event(&machine, STRANGE_RUNTIME_EVENT_HOLD, &now, &modes);
    }

    if (strange_state_machine_timeout_due(&machine, &now)) {
      if (handle_runtime_event(&machine, STRANGE_RUNTIME_EVENT_TIMEOUT, &now,
                               &modes) == -1) {
        goto cleanup;
      }
    } else if (machine.state == STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE &&
               render_frame(&machine, &now, &modes) == -1) {
      goto cleanup;
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
                                &modes) == -1) {
          goto cleanup;
        }
      } else if (bytes == 0) {
        handle_runtime_event(&machine, STRANGE_RUNTIME_EVENT_SHUTDOWN, &now,
                             &modes);
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
                                 &modes) == -1) {
          goto cleanup;
        }
        if (forward_output(&modes, buffer, (size_t)bytes) == -1) {
          goto cleanup;
        }
      } else if (bytes == 0 || (bytes < 0 && errno == EIO)) {
        handle_runtime_event(&machine, STRANGE_RUNTIME_EVENT_SHUTDOWN, &now,
                             &modes);
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
                           &modes);
      runtime_status = 0;
      break;
    }
  }

  status = runtime_status;

cleanup:
  if (machine.state == STRANGE_RUNTIME_STATE_SCREENSAVER_ACTIVE) {
    exit_screensaver(&modes);
  }
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
