#ifndef STRANGE_TERMINAL_MODES_H_
#define STRANGE_TERMINAL_MODES_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STRANGE_TERMINAL_MODES_MAX_PARAMETERS 8

/*
  Follows the few terminal modes the shell's output can change that decide how
  the screensaver takes over the screen and hands it back.
*/
struct strange_terminal_modes {
  int alternate_screen;
  int cursor_visible;
  enum {
    STRANGE_TERMINAL_MODES_TEXT = 0,
    STRANGE_TERMINAL_MODES_ESCAPE,
    STRANGE_TERMINAL_MODES_CSI,
  } parse_state;
  int private_sequence;
  int parameters[STRANGE_TERMINAL_MODES_MAX_PARAMETERS];
  size_t parameter_count;
};

void strange_terminal_modes_init(struct strange_terminal_modes *modes);
void strange_terminal_modes_write(struct strange_terminal_modes *modes,
                                  const char *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif
