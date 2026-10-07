#include <string.h>

#include "src/renderer.h"
#include "watermark.h"

void render_screensaver_watermark(struct ScreenBuffer *buffer, int preview) {
  static const char *session_lines[] = {
      " any key wakes ",
      " Ctrl-Q disables ",
  };
  static const char *preview_lines[] = {
      " any key exits ",
  };
  const char **lines = preview ? preview_lines : session_lines;
  size_t line_count = preview ? 1 : 2;

  if (buffer == NULL) {
    return;
  }

  for (size_t row = 0; row < line_count; ++row) {
    size_t line_length = strlen(lines[row]);
    int x = 0;

    if ((int)line_length < buffer->w) {
      x = buffer->w - (int)line_length;
    }

    write_string_to_buffer(buffer, lines[row], x, (int)row);
  }
}
