#include <string.h>

#include "src/renderer.h"
#include "watermark.h"

void render_screensaver_watermark(struct ScreenBuffer *buffer) {
  static const char *lines[] = {
      " any key wakes ",
      " Ctrl-Q disables ",
  };

  if (buffer == NULL) {
    return;
  }

  for (size_t row = 0; row < (sizeof(lines) / sizeof(lines[0])); ++row) {
    size_t line_length = strlen(lines[row]);
    int x = 0;

    if ((int)line_length < buffer->w) {
      x = buffer->w - (int)line_length;
    }

    write_string_to_buffer(buffer, lines[row], x, (int)row);
  }
}
