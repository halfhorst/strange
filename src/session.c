#include "session.h"

#include <stdlib.h>
#include <string.h>

#include "keys.h"

static const char *variable_or(const char *name, const char *fallback) {
  const char *value = getenv(name);

  return value != NULL && value[0] != '\0' ? value : fallback;
}

int strange_session_export(const char *screensaver_name, int timeout_seconds,
                           int cover_fullscreen, int disable_key) {
  char timeout[32];
  char key_label[16];

  snprintf(timeout, sizeof(timeout), "%d", timeout_seconds);
  strange_control_key_label(disable_key, key_label, sizeof(key_label));
  if (setenv(STRANGE_SESSION_SCREENSAVER_VARIABLE, screensaver_name, 1) == -1 ||
      setenv(STRANGE_SESSION_TIMEOUT_VARIABLE, timeout, 1) == -1 ||
      setenv(STRANGE_SESSION_COVER_FULLSCREEN_VARIABLE,
             cover_fullscreen ? "1" : "0", 1) == -1 ||
      setenv(STRANGE_SESSION_DISABLE_KEY_VARIABLE, key_label, 1) == -1) {
    return -1;
  }

  return 0;
}

int strange_session_is_current(const char *terminal_name) {
  const char *session_terminal = getenv(STRANGE_SESSION_TTY_VARIABLE);

  return terminal_name != NULL && session_terminal != NULL &&
         strcmp(session_terminal, terminal_name) == 0;
}

int strange_session_print_status(FILE *stream, const char *terminal_name) {
  if (!strange_session_is_current(terminal_name)) {
    fprintf(stream, "Not running under strange\n");
    return 1;
  }

  fprintf(stream, "Running under strange\n");
  fprintf(stream, "  screensaver: %s\n",
          variable_or(STRANGE_SESSION_SCREENSAVER_VARIABLE, "unknown"));
  fprintf(stream, "  timeout: %s seconds\n",
          variable_or(STRANGE_SESSION_TIMEOUT_VARIABLE, "unknown"));
  fprintf(stream, "  full-screen programs: %s\n",
          strcmp(variable_or(STRANGE_SESSION_COVER_FULLSCREEN_VARIABLE, "0"),
                 "1") == 0
              ? "covered"
              : "hold the screensaver off");
  fprintf(stream, "  disable key: %s\n",
          variable_or(STRANGE_SESSION_DISABLE_KEY_VARIABLE, "unknown"));
  return 0;
}
