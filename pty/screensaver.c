#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "pty.h"
#include "screensaver.h"
#include "watermark.h"

#define BUFFER_SIZE 1024 * 1024
#define DEFAULT_TIMEOUT_SECONDS 10

static struct termios orig_termios;
static int screensaver_active = 0;
static int raw_mode_enabled = 0;
static time_t last_activity;
static int timeout_seconds = DEFAULT_TIMEOUT_SECONDS;

extern int master_fd;
extern pid_t shell_pid;

void disable_raw_mode(void) {
  if (!raw_mode_enabled) {
    return;
  }

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
  raw_mode_enabled = 0;
}

void enable_raw_mode(void) {
  if (raw_mode_enabled) {
    return;
  }

  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disable_raw_mode);

  struct termios raw = orig_termios;
  cfmakeraw(&raw);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  raw_mode_enabled = 1;
}

void start_screensaver(void) {
  screensaver_active = 1;

  printf("\033[?1049h");   // Save screen and cursor
  printf("\033[2J\033[H"); // Clear screen
  printf("\033[?25l");     // Hide cursor

  time_t start_time = time(NULL);

  while (screensaver_active && !strange_shutdown_requested()) {
    time_t current_time = time(NULL);
    int elapsed = current_time - start_time;

    char spinner[] = "|/-\\";
    int spinner_idx = elapsed % 4;

    printf("\033[H"); // Move to top
    printf("Strange... %c\n", spinner[spinner_idx]);
    printf("Elapsed: %d seconds\n", elapsed);
    display_exit_instructions();

    fflush(stdout);

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    if (master_fd >= 0) {
      FD_SET(master_fd, &read_fds);
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 250000;

    int max_fd = STDIN_FILENO;
    if (master_fd > max_fd) {
      max_fd = master_fd;
    }

    int ready = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }

    if (poll_shell_exit(NULL) == 1) {
      break;
    }

    if (ready > 0) {
      break;
    }
  }

  stop_screensaver();
}

void stop_screensaver(void) {
  printf("\033[?25h");   // Show cursor
  printf("\033[?1049l"); // Restore screen and cursor
  fflush(stdout);

  last_activity = time(NULL);
  screensaver_active = 0;
}

void set_screensaver_timeout(int seconds) { timeout_seconds = seconds; }

int get_screensaver_timeout(void) { return timeout_seconds; }

void reset_activity_timer(void) { last_activity = time(NULL); }

int check_screensaver_timeout(void) {
  time_t current_time = time(NULL);
  return (current_time - last_activity) >= timeout_seconds;
}

int run_screensaver_loop(void) {
  reset_activity_timer();

  printf("Screensaver started\n");
  printf("Demo:  \n");
  printf("Timeout: %ds\n", timeout_seconds);
  printf("Press ??? to exit\n");
  printf("==================\n");
  printf("\n\n");
  fflush(stdout);

  enable_raw_mode();

  char buffer[BUFFER_SIZE];
  fd_set read_fds;

  while (!strange_shutdown_requested()) {
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(master_fd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 100ms

    int result = select(master_fd + 1, &read_fds, NULL, NULL, &timeout);

    if (result < 0 && errno != EINTR) {
      perror("select");
      return 1;
    }

    if (FD_ISSET(STDIN_FILENO, &read_fds)) {
      ssize_t bytes = read(STDIN_FILENO, buffer, sizeof(buffer));
      if (bytes > 0) {
        reset_activity_timer();
        write(master_fd, buffer, bytes);
      } else if (bytes == 0) {
        break;
      }
    }

    if (FD_ISSET(master_fd, &read_fds)) {
      ssize_t bytes = read(master_fd, buffer, sizeof(buffer));
      if (bytes > 0) {
        reset_activity_timer();
        write(STDOUT_FILENO, buffer, bytes);
      } else if (bytes == 0 || (bytes < 0 && errno == EIO)) {
        break;
      }
    }

    int shell_exited = poll_shell_exit(NULL);
    if (shell_exited == -1) {
      perror("waitpid");
      return 1;
    }
    if (shell_exited == 1) {
      break;
    }

    if (check_screensaver_timeout()) {
      start_screensaver();
    }
  }

  return 0;
}
