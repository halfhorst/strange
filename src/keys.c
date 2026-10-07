#include "keys.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int has_prefix_ignoring_case(const char *text, const char *prefix) {
  for (; *prefix != '\0'; ++text, ++prefix) {
    if (tolower((unsigned char)*text) != *prefix) {
      return 0;
    }
  }

  return 1;
}

int strange_parse_control_key(const char *text, int *key) {
  int letter = 0;

  if (text == NULL || key == NULL) {
    return -1;
  }
  if (has_prefix_ignoring_case(text, "none") && text[4] == '\0') {
    *key = STRANGE_NO_KEY;
    return 0;
  }

  if (has_prefix_ignoring_case(text, "ctrl-") ||
      has_prefix_ignoring_case(text, "ctrl+")) {
    text += 5;
  } else if (text[0] == '^') {
    text += 1;
  } else {
    return -1;
  }

  letter = tolower((unsigned char)text[0]);
  if (letter < 'a' || letter > 'z' || text[1] != '\0' || letter == 'i' ||
      letter == 'j' || letter == 'm') {
    return -1;
  }

  *key = letter - 'a' + 1;
  return 0;
}

void strange_control_key_label(int key, char *label, size_t label_size) {
  if (key < 1 || key > 26) {
    snprintf(label, label_size, "none");
    return;
  }

  snprintf(label, label_size, "Ctrl-%c", 'A' + key - 1);
}
