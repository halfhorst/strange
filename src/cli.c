#include "cli.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screensaver_registry.h"

static void set_error(char *buffer, size_t buffer_size, const char *format, ...) {
  va_list args;

  if (buffer == NULL || buffer_size == 0) {
    return;
  }

  va_start(args, format);
  vsnprintf(buffer, buffer_size, format, args);
  va_end(args);
}

static int parse_positive_seconds(const char *value, int *timeout_seconds) {
  char *end = NULL;
  long parsed = 0;

  if (value == NULL || timeout_seconds == NULL) {
    return -1;
  }

  parsed = strtol(value, &end, 10);
  if (end == value || *end != '\0' || parsed <= 0 || parsed > INT_MAX) {
    return -1;
  }

  *timeout_seconds = (int)parsed;
  return 0;
}

int strange_cli_parse(int argc, char *argv[], struct strange_cli_options *options,
                      char *error_buffer, size_t error_buffer_size) {
  int saw_timeout = 0;

  if (options == NULL || argc < 1 || argv == NULL || argv[0] == NULL) {
    set_error(error_buffer, error_buffer_size, "invalid CLI arguments");
    return -1;
  }

  memset(options, 0, sizeof(*options));
  options->command = STRANGE_CLI_COMMAND_RUN_NAMED;
  options->timeout_seconds = STRANGE_DEFAULT_TIMEOUT_SECONDS;

  if (argc == 2 &&
      (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
    options->command = STRANGE_CLI_COMMAND_HELP;
    return 0;
  }

  for (int index = 1; index < argc; ++index) {
    if (strcmp(argv[index], "--timeout") == 0) {
      if (saw_timeout) {
        set_error(error_buffer, error_buffer_size,
                  "`--timeout` may only be provided once");
        return -1;
      }
      if (options->screensaver_name != NULL ||
          options->command == STRANGE_CLI_COMMAND_RUN_RANDOM ||
          options->command == STRANGE_CLI_COMMAND_LIST) {
        set_error(error_buffer, error_buffer_size,
                  "`--timeout` only supports `strange --timeout <seconds> "
                  "<screensaver-name>`");
        return -1;
      }
      if (index + 1 >= argc ||
          parse_positive_seconds(argv[index + 1], &options->timeout_seconds) !=
              0) {
        set_error(error_buffer, error_buffer_size,
                  "timeout must be a positive integer greater than 0");
        return -1;
      }
      saw_timeout = 1;
      ++index;
      continue;
    }

    if (strcmp(argv[index], "--list") == 0) {
      if (index != 1 || argc != 2) {
        set_error(error_buffer, error_buffer_size,
                  "`--list` does not accept additional arguments");
        return -1;
      }
      options->command = STRANGE_CLI_COMMAND_LIST;
      return 0;
    }

    if (strcmp(argv[index], "--random") == 0) {
      if (index != 1 ||
          options->timeout_seconds != STRANGE_DEFAULT_TIMEOUT_SECONDS) {
        set_error(error_buffer, error_buffer_size,
                  "`--random` only supports `strange --random "
                  "<screensaver-name>...`");
        return -1;
      }
      if (index + 1 >= argc) {
        set_error(error_buffer, error_buffer_size,
                  "`--random` requires at least one screensaver name");
        return -1;
      }

      options->command = STRANGE_CLI_COMMAND_RUN_RANDOM;
      options->random_names = (const char *const *)&argv[index + 1];
      options->random_name_count = (size_t)(argc - index - 1);

      for (size_t random_index = 0; random_index < options->random_name_count;
           ++random_index) {
        if (options->random_names[random_index][0] == '-') {
          set_error(error_buffer, error_buffer_size,
                    "unexpected option in `--random` list: %s",
                    options->random_names[random_index]);
          return -1;
        }
      }

      return 0;
    }

    if (argv[index][0] == '-') {
      set_error(error_buffer, error_buffer_size, "unknown option: %s",
                argv[index]);
      return -1;
    }

    if (options->screensaver_name != NULL) {
      set_error(error_buffer, error_buffer_size,
                "expected exactly one screensaver name");
      return -1;
    }

    options->screensaver_name = argv[index];
  }

  if (options->screensaver_name == NULL) {
    set_error(error_buffer, error_buffer_size,
              "expected a screensaver name or `--list`");
    return -1;
  }

  return 0;
}

int strange_cli_resolve_screensaver(
    const struct strange_cli_options *options,
    const struct strange_screensaver_descriptor **descriptor,
    char *error_buffer, size_t error_buffer_size) {
  struct strange_screensaver_catalog catalog = {0};
  const struct strange_screensaver_record *record = NULL;

  if (options == NULL || descriptor == NULL) {
    set_error(error_buffer, error_buffer_size, "invalid CLI resolution request");
    return -1;
  }

  *descriptor = NULL;
  if (options->command == STRANGE_CLI_COMMAND_HELP ||
      options->command == STRANGE_CLI_COMMAND_LIST) {
    return 0;
  }

  if (strange_screensaver_catalog_init(&catalog, error_buffer,
                                       error_buffer_size) == -1) {
    return -1;
  }

  if (options->command == STRANGE_CLI_COMMAND_RUN_NAMED) {
    record = strange_screensaver_catalog_find(&catalog, options->screensaver_name);
    if (record == NULL) {
      set_error(error_buffer, error_buffer_size, "unknown screensaver: %s",
                options->screensaver_name);
      strange_screensaver_catalog_free(&catalog);
      return -1;
    }
    if (record->source != STRANGE_SCREENSAVER_RECORD_SOURCE_BUILTIN) {
      set_error(error_buffer, error_buffer_size,
                "user screensaver loading is not implemented yet: %s",
                options->screensaver_name);
      strange_screensaver_catalog_free(&catalog);
      return -1;
    }
    *descriptor = record->descriptor;
    strange_screensaver_catalog_free(&catalog);
    return 0;
  }

  if (options->command == STRANGE_CLI_COMMAND_RUN_RANDOM) {
    for (size_t index = 0; index < options->random_name_count; ++index) {
      if (strange_screensaver_catalog_find(&catalog, options->random_names[index]) ==
          NULL) {
        set_error(error_buffer, error_buffer_size, "unknown screensaver: %s",
                  options->random_names[index]);
        strange_screensaver_catalog_free(&catalog);
        return -1;
      }
    }

    record = strange_screensaver_catalog_find(
        &catalog, options->random_names[rand() % options->random_name_count]);
    if (record == NULL) {
      set_error(error_buffer, error_buffer_size, "unsupported CLI command");
      strange_screensaver_catalog_free(&catalog);
      return -1;
    }
    if (record->source != STRANGE_SCREENSAVER_RECORD_SOURCE_BUILTIN) {
      set_error(error_buffer, error_buffer_size,
                "user screensaver loading is not implemented yet: %s",
                record->name);
      strange_screensaver_catalog_free(&catalog);
      return -1;
    }
    *descriptor = record->descriptor;
    strange_screensaver_catalog_free(&catalog);
    return 0;
  }

  strange_screensaver_catalog_free(&catalog);
  set_error(error_buffer, error_buffer_size, "unsupported CLI command");
  return -1;
}

void strange_cli_print_usage(FILE *stream, const char *prog_name) {
  fprintf(stream, "Usage: %s <screensaver-name>\n", prog_name);
  fprintf(stream, "       %s --random <screensaver-name>...\n", prog_name);
  fprintf(stream, "       %s --timeout <seconds> <screensaver-name>\n",
          prog_name);
  fprintf(stream, "       %s --list\n", prog_name);
  fprintf(stream, "       %s -h | --help\n", prog_name);
  fprintf(stream, "\n");
  fprintf(stream, "  --timeout seconds: inactivity before the screensaver "
                  "starts (default: %d)\n",
          STRANGE_DEFAULT_TIMEOUT_SECONDS);
}

int strange_cli_print_list(FILE *stream, char *error_buffer,
                           size_t error_buffer_size) {
  struct strange_screensaver_catalog catalog = {0};
  size_t count = 0;
  const struct strange_screensaver_record *records = NULL;

  if (stream == NULL) {
    set_error(error_buffer, error_buffer_size, "invalid list output stream");
    return -1;
  }

  if (strange_screensaver_catalog_init(&catalog, error_buffer,
                                       error_buffer_size) == -1) {
    return -1;
  }

  records = strange_screensaver_catalog_records(&catalog, &count);

  fprintf(stream, "Built-in screensavers:\n");
  for (size_t index = 0; index < count; ++index) {
    if (records[index].source == STRANGE_SCREENSAVER_RECORD_SOURCE_BUILTIN) {
      fprintf(stream, "  %s\n", records[index].name);
    }
  }

  fprintf(stream, "User screensavers:\n");
  for (size_t index = 0; index < count; ++index) {
    if (records[index].source != STRANGE_SCREENSAVER_RECORD_SOURCE_BUILTIN) {
      fprintf(stream, "  %s\n", records[index].name);
    }
  }

  strange_screensaver_catalog_free(&catalog);
  return 0;
}
