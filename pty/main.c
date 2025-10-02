#include "pty.h"
#include "screensaver.h"
#include "timer.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void print_usage(const char *prog_name) {
  printf("Usage: %s [timeout_seconds]\n", prog_name);
  printf("  timeout_seconds: Number of seconds of inactivity before "
         "screensaver starts (default: 10)\n");
  printf("  -h, --help: Show this help message\n");
}

int main(int argc, char *argv[]) {
  int timeout_seconds = 10; // Default timeout

  // Parse command line arguments
  if (argc > 1) {
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
      print_usage(argv[0]);
      return 0;
    }
    timeout_seconds = atoi(argv[1]);
    if (timeout_seconds <= 0) {
      fprintf(stderr, "Invalid timeout value. Using default: 10 seconds\n");
      timeout_seconds = 10;
    }
  }

  // Set up PTY and shell
  if (setup_pty_and_shell() < 0) {
    fprintf(stderr, "Failed to set up PTY and shell\n");
    return 1;
  }

  // Make stdin non-blocking
  int flags = fcntl(STDIN_FILENO, F_GETFL);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

  // Configure screensaver timeout
  set_screensaver_timeout(timeout_seconds);

  // Run the main screensaver loop
  return run_screensaver_loop();
}
