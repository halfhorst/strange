#ifndef STRANGE_KEYS_H_
#define STRANGE_KEYS_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STRANGE_DEFAULT_DISABLE_KEY 0x11
#define STRANGE_NO_KEY 0

/*
  Reads a control key written as `ctrl-q`, `ctrl+q` or `^q`, in either case,
  into the byte a terminal sends for it. `none` gives STRANGE_NO_KEY. Tab,
  Enter and newline are refused since a shell cannot do without them.
*/
int strange_parse_control_key(const char *text, int *key);

// Writes `Ctrl-Q` for a key, or `none`.
void strange_control_key_label(int key, char *label, size_t label_size);

#ifdef __cplusplus
}
#endif

#endif
