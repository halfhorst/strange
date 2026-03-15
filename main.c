#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pty/runtime.h"

static int parse_timeout_seconds(int argc, char *argv[], int *timeout_seconds) {
  if (argc <= 1) {
    return 0;
  }

  if (argc > 2) {
    return -1;
  }

  char *end = NULL;
  long parsed = strtol(argv[1], &end, 10);
  if (end == argv[1] || *end != '\0' || parsed <= 0 || parsed > INT_MAX) {
    return -1;
  }

  *timeout_seconds = (int)parsed;
  return 0;
}

static int validate_interactive_tty(void) {
  if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
    fprintf(stderr, "strange requires an interactive TTY on stdin and stdout\n");
    return -1;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  struct strange_options options = {
      .timeout_seconds = STRANGE_DEFAULT_TIMEOUT_SECONDS,
  };

  if (argc > 1 &&
      (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
    strange_print_usage(stdout, argv[0]);
    return EXIT_SUCCESS;
  }

  if (parse_timeout_seconds(argc, argv, &options.timeout_seconds) != 0) {
    strange_print_usage(stderr, argv[0]);
    return EXIT_FAILURE;
  }

  if (validate_interactive_tty() != 0) {
    return EXIT_FAILURE;
  }

  return strange_run(&options);
}
