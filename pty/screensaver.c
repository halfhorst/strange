#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "pty.h"
#include "screensaver.h"
#include "timer.h"
#include "watermark.h"

#define BUFFER_SIZE 1024 * 1024
#define DEFAULT_TIMEOUT_SECONDS 10

static struct termios orig_termios;
static int screensaver_active = 0;
static int should_exit = 0;
static time_t last_activity;
static int timeout_seconds = DEFAULT_TIMEOUT_SECONDS;

// External PTY variables that will be managed by pty.c
extern int master_fd;
extern pid_t shell_pid;

void disable_raw_mode(void) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode(void) {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disable_raw_mode);

  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void cleanup_screensaver(int sig) {
  // Ctrl+C just stops the screensaver, doesn't exit the program
  if (screensaver_active) {
    stop_screensaver();
  }
  // To exit the program, use Ctrl+D during screensaver
}

void start_screensaver(void) {
  screensaver_active = 1;

  printf("\033[?1049h");   // Save screen and cursor
  printf("\033[2J\033[H"); // Clear screen
  printf("\033[?25l");     // Hide cursor

  enable_raw_mode();

  time_t start_time = time(NULL);

  while (screensaver_active && !should_exit) {
    time_t current_time = time(NULL);
    int elapsed = current_time - start_time;

    // Simple screensaver: show elapsed time and a rotating pattern
    char spinner[] = "|/-\\";
    int spinner_idx = elapsed % 4;

    printf("\033[H"); // Move to top
    printf("Strange... %c\n", spinner[spinner_idx]);
    printf("Elapsed: %d seconds\n", elapsed);
    display_exit_instructions();

    fflush(stdout);
    usleep(250000); // 250ms delay

    // Check for any input
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    if (select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &tv) > 0) {
      char buffer[BUFFER_SIZE];
      ssize_t bytes = read(STDIN_FILENO, buffer, sizeof(buffer));

      if (bytes > 0) {
        // Check for special exit sequence: Ctrl+\ then 0
        static char last_key = 0;
        if (last_key == 28 && buffer[0] == '0') { // Ctrl+\ is ASCII 28
          should_exit = 1;
        }
        last_key = (bytes == 1) ? buffer[0] : 0;
      }
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

  disable_raw_mode();
  screensaver_active = 0;
}

void set_screensaver_timeout(int seconds) { timeout_seconds = seconds; }

int get_screensaver_timeout(void) { return timeout_seconds; }

void reset_activity_timer(void) { last_activity = time(NULL); }

int check_screensaver_timeout(void) {
  time_t current_time = time(NULL);
  return (current_time - last_activity) >= timeout_seconds;
}

// Forwards terminal behavior and monitors
// activity
int run_screensaver_loop(void) {
  // Set up signal handlers
  signal(SIGINT, cleanup_screensaver);
  signal(SIGTERM, cleanup_screensaver);

  reset_activity_timer();

  printf("Started your terminal screen saver\n");
  printf("Demo loaded:  \n");
  printf("Timeout: %d seconds\n", timeout_seconds);
  printf("Press ??? to exit.\n");
  printf("===================================");
  printf("\n\n");
  fflush(stdout);

  char buffer[BUFFER_SIZE];
  fd_set read_fds;

  while (!should_exit) {
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

    // Check for user input from terminal
    if (FD_ISSET(STDIN_FILENO, &read_fds)) {
      ssize_t bytes = read(STDIN_FILENO, buffer, sizeof(buffer));
      if (bytes > 0) {
        reset_activity_timer();
        write(master_fd, buffer, bytes);
      }
    }

    // Check for output from shell
    if (FD_ISSET(master_fd, &read_fds)) {
      ssize_t bytes = read(master_fd, buffer, sizeof(buffer));
      if (bytes > 0) {
        reset_activity_timer();
        write(STDOUT_FILENO, buffer, bytes);
      }
    }

    // Check for timeout
    if (check_screensaver_timeout()) {
      start_screensaver();
    }

    // Check if shell process is still alive
    int status;
    if (waitpid(shell_pid, &status, WNOHANG) > 0) {
      break;
    }
  }

  return 0;
}
