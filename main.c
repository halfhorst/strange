#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "pty/runtime.h"
#include "src/cli.h"

static int validate_interactive_tty(void) {
  if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
    fprintf(stderr, "strange requires an interactive TTY on stdin and stdout\n");
    return -1;
  }

  return 0;
}

static void seed_random_selection(void) {
  struct timespec now = {0, 0};
  unsigned int seed = (unsigned int)getpid();

  if (clock_gettime(CLOCK_REALTIME, &now) == 0) {
    seed ^= (unsigned int)now.tv_sec;
    seed ^= (unsigned int)now.tv_nsec;
  } else {
    seed ^= (unsigned int)time(NULL);
  }

  srand(seed);
}

int main(int argc, char *argv[]) {
  struct strange_cli_options cli_options = {
      .command = STRANGE_CLI_COMMAND_RUN_NAMED,
      .timeout_seconds = STRANGE_DEFAULT_TIMEOUT_SECONDS,
      .screensaver_name = NULL,
      .random_names = NULL,
      .random_name_count = 0,
  };
  struct strange_options options = {
      .timeout_seconds = STRANGE_DEFAULT_TIMEOUT_SECONDS,
      .cover_fullscreen = 0,
      .start_now = 0,
      .screensaver_descriptor = NULL,
  };
  char error_buffer[256] = {0};

  if (strange_cli_parse(argc, argv, &cli_options, error_buffer,
                        sizeof(error_buffer)) != 0) {
    fprintf(stderr, "%s\n", error_buffer);
    strange_cli_print_usage(stderr, argv[0]);
    return EXIT_FAILURE;
  }

  if (cli_options.command == STRANGE_CLI_COMMAND_HELP) {
    strange_cli_print_usage(stdout, argv[0]);
    return EXIT_SUCCESS;
  }
  if (cli_options.command == STRANGE_CLI_COMMAND_LIST) {
    if (strange_cli_print_list(stdout, error_buffer, sizeof(error_buffer)) != 0) {
      fprintf(stderr, "%s\n", error_buffer);
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }

  if (cli_options.command == STRANGE_CLI_COMMAND_RUN_RANDOM) {
    seed_random_selection();
  }
  if (strange_cli_resolve_screensaver(&cli_options, &options.screensaver_descriptor,
                                      error_buffer, sizeof(error_buffer)) != 0) {
    fprintf(stderr, "%s\n", error_buffer);
    return EXIT_FAILURE;
  }
  options.timeout_seconds = cli_options.timeout_seconds;
  options.cover_fullscreen = cli_options.cover_fullscreen;
  options.start_now = cli_options.start_now;

  if (validate_interactive_tty() != 0) {
    return EXIT_FAILURE;
  }

  return strange_run(&options);
}
