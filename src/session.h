#ifndef STRANGE_SESSION_H_
#define STRANGE_SESSION_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
  The shell that strange starts is given these variables so that programs in
  it, strange included, can tell they are in a wrapped terminal and how it was
  configured. They describe how the session started and are not updated.
*/
#define STRANGE_SESSION_TTY_VARIABLE "STRANGE_TTY"
#define STRANGE_SESSION_SCREENSAVER_VARIABLE "STRANGE_SCREENSAVER"
#define STRANGE_SESSION_TIMEOUT_VARIABLE "STRANGE_TIMEOUT"
#define STRANGE_SESSION_COVER_FULLSCREEN_VARIABLE "STRANGE_COVER_FULLSCREEN"

int strange_session_export(const char *screensaver_name, int timeout_seconds,
                           int cover_fullscreen);

/*
  The variables are inherited by anything started from the wrapped shell,
  including other terminal windows, so a session only counts when it names the
  terminal being asked about.
*/
int strange_session_is_current(const char *terminal_name);

// Describes the session on `terminal_name`. Returns 0 if there is one and 1 if
// not.
int strange_session_print_status(FILE *stream, const char *terminal_name);

#ifdef __cplusplus
}
#endif

#endif
