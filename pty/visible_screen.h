#ifndef STRANGE_VISIBLE_SCREEN_H_
#define STRANGE_VISIBLE_SCREEN_H_

#include <stddef.h>
#include <stdio.h>

#include "src/renderer.h"

struct strange_visible_screen {
  struct ScreenBuffer current;
  struct ScreenBuffer snapshot;
  int cursor_x;
  int cursor_y;
  int saved_cursor_x;
  int saved_cursor_y;
  int saved_cursor_ready;
  int cursor_visible;
  int snapshot_cursor_x;
  int snapshot_cursor_y;
  int snapshot_cursor_visible;
  int snapshot_valid;
  enum {
    STRANGE_VISIBLE_SCREEN_TEXT = 0,
    STRANGE_VISIBLE_SCREEN_ESCAPE,
    STRANGE_VISIBLE_SCREEN_CSI,
    STRANGE_VISIBLE_SCREEN_OSC,
    STRANGE_VISIBLE_SCREEN_OSC_ESC,
  } parse_state;
  char csi_buffer[32];
  size_t csi_length;
};

int strange_visible_screen_init(struct strange_visible_screen *screen, int w,
                                int h);
void strange_visible_screen_destroy(struct strange_visible_screen *screen);
int strange_visible_screen_write(struct strange_visible_screen *screen,
                                 const char *data, size_t length);
int strange_visible_screen_capture_snapshot(
    struct strange_visible_screen *screen);
void strange_visible_screen_discard_snapshot(
    struct strange_visible_screen *screen);
int strange_visible_screen_resize(struct strange_visible_screen *screen, int w,
                                  int h, int clear_current,
                                  int discard_snapshot);
int strange_visible_screen_restore(struct strange_visible_screen *screen,
                                   FILE *stream);
int strange_visible_screen_restore_to_fd(struct strange_visible_screen *screen,
                                         int fd);

#endif
