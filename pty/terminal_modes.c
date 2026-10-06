#include "terminal_modes.h"

#include <string.h>

static void set_private_mode(struct strange_terminal_modes *modes, int mode,
                             int enabled) {
  switch (mode) {
  case 25:
    modes->cursor_visible = enabled;
    return;
  case 47:
  case 1047:
  case 1049:
    modes->alternate_screen = enabled;
    return;
  default:
    return;
  }
}

static void finish_csi(struct strange_terminal_modes *modes,
                       unsigned char final) {
  if (!modes->private_sequence || (final != 'h' && final != 'l')) {
    return;
  }

  for (size_t index = 0; index <= modes->parameter_count &&
                         index < STRANGE_TERMINAL_MODES_MAX_PARAMETERS;
       ++index) {
    set_private_mode(modes, modes->parameters[index], final == 'h');
  }
}

static void process_byte(struct strange_terminal_modes *modes,
                         unsigned char byte) {
  switch (modes->parse_state) {
  case STRANGE_TERMINAL_MODES_TEXT:
    if (byte == '\033') {
      modes->parse_state = STRANGE_TERMINAL_MODES_ESCAPE;
    }
    return;
  case STRANGE_TERMINAL_MODES_ESCAPE:
    if (byte == '\033') {
      return;
    }
    modes->parse_state = STRANGE_TERMINAL_MODES_TEXT;
    if (byte == '[') {
      modes->parse_state = STRANGE_TERMINAL_MODES_CSI;
      modes->private_sequence = 0;
      modes->parameter_count = 0;
      memset(modes->parameters, 0, sizeof(modes->parameters));
    } else if (byte == 'c') {
      modes->alternate_screen = 0;
      modes->cursor_visible = 1;
    }
    return;
  case STRANGE_TERMINAL_MODES_CSI:
    if (byte == '\033') {
      modes->parse_state = STRANGE_TERMINAL_MODES_ESCAPE;
    } else if (byte >= 0x40 && byte <= 0x7e) {
      finish_csi(modes, byte);
      modes->parse_state = STRANGE_TERMINAL_MODES_TEXT;
    } else if (byte == '?') {
      modes->private_sequence = 1;
    } else if (byte == ';') {
      modes->parameter_count++;
    } else if (byte >= '0' && byte <= '9' &&
               modes->parameter_count < STRANGE_TERMINAL_MODES_MAX_PARAMETERS) {
      int *parameter = &modes->parameters[modes->parameter_count];
      if (*parameter < 100000) {
        *parameter = (*parameter * 10) + (byte - '0');
      }
    }
    return;
  }
}

void strange_terminal_modes_init(struct strange_terminal_modes *modes) {
  memset(modes, 0, sizeof(*modes));
  modes->cursor_visible = 1;
}

void strange_terminal_modes_write(struct strange_terminal_modes *modes,
                                  const char *data, size_t length) {
  for (size_t index = 0; index < length; ++index) {
    process_byte(modes, (unsigned char)data[index]);
  }
}
