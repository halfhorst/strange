#include "cli.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "keys.h"
#include "screensaver_loader.h"
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

static int reject_options_with_preview(const struct strange_cli_options *options,
                                       int saw_session_option,
                                       char *error_buffer,
                                       size_t error_buffer_size) {
  if (options->preview && (saw_session_option || options->cover_fullscreen)) {
    set_error(error_buffer, error_buffer_size,
              "`--now` runs only the screensaver, so `--timeout`, "
              "`--cover-fullscreen` and `--disable-key` do not apply");
    return -1;
  }

  return 0;
}

int strange_cli_parse(int argc, char *argv[], struct strange_cli_options *options,
                      char *error_buffer, size_t error_buffer_size) {
  int saw_timeout = 0;
  int leading_flag_count = 0;
  int saw_disable_key = 0;

  if (options == NULL || argc < 1 || argv == NULL || argv[0] == NULL) {
    set_error(error_buffer, error_buffer_size, "invalid CLI arguments");
    return -1;
  }

  memset(options, 0, sizeof(*options));
  options->command = STRANGE_CLI_COMMAND_RUN_NAMED;
  options->timeout_seconds = STRANGE_DEFAULT_TIMEOUT_SECONDS;
  options->disable_key = STRANGE_DEFAULT_DISABLE_KEY;

  if (argc == 2 &&
      (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
    options->command = STRANGE_CLI_COMMAND_HELP;
    return 0;
  }

  for (int index = 1; index < argc; ++index) {
    if (strcmp(argv[index], "--cover-fullscreen") == 0 ||
        strcmp(argv[index], "--now") == 0) {
      if (index != 1 + leading_flag_count) {
        set_error(error_buffer, error_buffer_size,
                  "`%s` must come before the other arguments", argv[index]);
        return -1;
      }
      if (strcmp(argv[index], "--now") == 0) {
        options->preview = 1;
      } else {
        options->cover_fullscreen = 1;
      }
      leading_flag_count++;
      continue;
    }

    if (strcmp(argv[index], "--disable-key") == 0) {
      if (index != 1 + leading_flag_count) {
        set_error(error_buffer, error_buffer_size,
                  "`--disable-key` must come before the other arguments");
        return -1;
      }
      if (index + 1 >= argc ||
          strange_parse_control_key(argv[index + 1], &options->disable_key) !=
              0) {
        set_error(error_buffer, error_buffer_size,
                  "`--disable-key` takes a key such as `ctrl-q`, or `none`; "
                  "ctrl-i, ctrl-j and ctrl-m cannot be used");
        return -1;
      }
      saw_disable_key = 1;
      leading_flag_count += 2;
      ++index;
      continue;
    }

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

    if (strcmp(argv[index], "--status") == 0) {
      if (index != 1 || argc != 2) {
        set_error(error_buffer, error_buffer_size,
                  "`--status` does not accept additional arguments");
        return -1;
      }
      options->command = STRANGE_CLI_COMMAND_STATUS;
      return 0;
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
      if (index != 1 + leading_flag_count ||
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

      if (reject_options_with_preview(options, saw_disable_key, error_buffer,
                                      error_buffer_size) == -1) {
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

  return reject_options_with_preview(options, saw_timeout || saw_disable_key,
                                     error_buffer, error_buffer_size);
}

int strange_cli_resolve_screensaver(
    const struct strange_cli_options *options,
    const struct strange_screensaver_descriptor **descriptor,
    char *error_buffer, size_t error_buffer_size) {
  struct strange_screensaver_catalog catalog = {0};
  const struct strange_screensaver_record *record = NULL;
  int result = 0;

  if (options == NULL || descriptor == NULL) {
    set_error(error_buffer, error_buffer_size, "invalid CLI resolution request");
    return -1;
  }

  *descriptor = NULL;
  if (options->command == STRANGE_CLI_COMMAND_HELP ||
      options->command == STRANGE_CLI_COMMAND_LIST ||
      options->command == STRANGE_CLI_COMMAND_STATUS) {
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
    result = strange_screensaver_load(record, descriptor, error_buffer,
                                      error_buffer_size);
    strange_screensaver_catalog_free(&catalog);
    return result;
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
    result = strange_screensaver_load(record, descriptor, error_buffer,
                                      error_buffer_size);
    strange_screensaver_catalog_free(&catalog);
    return result;
  }

  strange_screensaver_catalog_free(&catalog);
  set_error(error_buffer, error_buffer_size, "unsupported CLI command");
  return -1;
}

void strange_cli_print_usage(FILE *stream, const char *prog_name) {
  fprintf(stream, "Usage: %s [options] <screensaver-name>\n", prog_name);
  fprintf(stream, "       %s [options] --random <screensaver-name>...\n",
          prog_name);
  fprintf(stream, "       %s [options] --timeout <seconds> "
                  "<screensaver-name>\n",
          prog_name);
  fprintf(stream, "       %s --now <screensaver-name>\n", prog_name);
  fprintf(stream, "       %s --now --random <screensaver-name>...\n",
          prog_name);
  fprintf(stream, "       %s --list\n", prog_name);
  fprintf(stream, "       %s --status\n", prog_name);
  fprintf(stream, "       %s -h | --help\n", prog_name);
  fprintf(stream, "\n");
  fprintf(stream, "  --timeout seconds: inactivity before the screensaver "
                  "starts (default: %d)\n",
          STRANGE_DEFAULT_TIMEOUT_SECONDS);
  fprintf(stream, "  --status: say whether this terminal is running under "
                  "strange and how it\n"
                  "            was started; exits 0 if it is and 1 if not\n");
  fprintf(stream, "  --now: run only the screensaver, without a shell, "
                  "until a key is pressed\n");
  fprintf(stream, "\nOptions, which come first:\n");
  fprintf(stream, "  --disable-key key: the key that turns the screensaver off "
                  "for the session,\n"
                  "                     such as ctrl-g, or none to reserve no "
                  "key (default: ctrl-q)\n");
  fprintf(stream, "  --cover-fullscreen: also start over full-screen programs "
                  "such as vim or top,\n"
                  "                      which otherwise hold the screensaver "
                  "off while they run\n");
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
