#ifndef STRANGE_CLI_H_
#define STRANGE_CLI_H_

#include <stddef.h>
#include <stdio.h>

#include "pty/runtime.h"
#include "screensaver_registry.h"

enum strange_cli_command {
  STRANGE_CLI_COMMAND_RUN_NAMED = 0,
  STRANGE_CLI_COMMAND_RUN_RANDOM,
  STRANGE_CLI_COMMAND_LIST,
  STRANGE_CLI_COMMAND_HELP,
};

struct strange_cli_options {
  enum strange_cli_command command;
  int timeout_seconds;
  const char *screensaver_name;
  const char *const *random_names;
  size_t random_name_count;
};

int strange_cli_parse(int argc, char *argv[], struct strange_cli_options *options,
                      char *error_buffer, size_t error_buffer_size);
int strange_cli_resolve_screensaver(
    const struct strange_cli_options *options,
    const struct strange_screensaver_descriptor **descriptor,
    char *error_buffer, size_t error_buffer_size);
void strange_cli_print_usage(FILE *stream, const char *prog_name);
int strange_cli_print_list(FILE *stream, char *error_buffer,
                           size_t error_buffer_size);

#endif
