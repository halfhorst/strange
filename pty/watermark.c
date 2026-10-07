#include <stdio.h>
#include <string.h>

#include "src/keys.h"
#include "src/renderer.h"
#include "watermark.h"

static void write_right_aligned(struct ScreenBuffer *buffer, const char *line,
                                int row) {
  int length = (int)strlen(line);

  write_string_to_buffer(buffer, line, length < buffer->w ? buffer->w - length : 0,
                         row);
}

void render_screensaver_watermark(struct ScreenBuffer *buffer, int preview,
                                  int disable_key) {
  char key_label[16];
  char disable_line[40];

  if (buffer == NULL) {
    return;
  }
  if (preview) {
    write_right_aligned(buffer, " any key exits ", 0);
    return;
  }

  write_right_aligned(buffer, " any key wakes ", 0);
  if (disable_key != STRANGE_NO_KEY) {
    strange_control_key_label(disable_key, key_label, sizeof(key_label));
    snprintf(disable_line, sizeof(disable_line), " %s disables ", key_label);
    write_right_aligned(buffer, disable_line, 1);
  }
}
