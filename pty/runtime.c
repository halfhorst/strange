#include "runtime.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "pty.h"
#include "screensaver.h"

void strange_print_usage(FILE *stream, const char *prog_name) {
  fprintf(stream, "Usage: %s [timeout_seconds]\n", prog_name);
  fprintf(stream,
          "  timeout_seconds: seconds of inactivity before the screensaver "
          "starts (default: %d)\n",
          STRANGE_DEFAULT_TIMEOUT_SECONDS);
  fprintf(stream, "  -h, --help: show this help message\n");
}

int strange_run(const struct strange_options *options) {
  if (setup_pty_and_shell() < 0) {
    fprintf(stderr, "Failed to set up PTY and shell\n");
    return 1;
  }

  int flags = fcntl(STDIN_FILENO, F_GETFL);
  if (flags == -1 || fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl");
    return 1;
  }

  set_screensaver_timeout(options->timeout_seconds);
  return run_screensaver_loop();
}
