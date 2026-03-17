#include <stdio.h>

#ifndef RENDERER_H_
#define RENDERER_H_

// The padding character used by the renderer. Non-printing C0 control seems to
// have inconsistent behavior. I trust \0 to never represent a character. The
// dirty trick is printing past it when rendering.
#define SL_PAD_CHAR 0x0

// The space character used by the renderer for creating an empty screen
#define SL_SPACE_CHAR 0x20

struct ScreenBuffer {
  int w;
  int h;
  char *buffer;
  int character_width;  // the number of characters reserved for each (x, y)
};

struct strange_render_context {
  struct ScreenBuffer buffer;
  int terminal_fd;
  FILE *stream;
  unsigned long frame_count;
};

int strange_get_terminal_size(int fd, int *w, int *h);
int strange_screen_buffer_init(struct ScreenBuffer *buffer, int w, int h,
                               int character_width);
int strange_screen_buffer_resize(struct ScreenBuffer *buffer, int w, int h);
void strange_screen_buffer_free(struct ScreenBuffer *buffer);
void strange_screen_buffer_clear(struct ScreenBuffer *buffer);
int strange_render_context_init(struct strange_render_context *context,
                                int terminal_fd, FILE *stream,
                                int character_width);
int strange_render_context_refresh_size(struct strange_render_context *context);
void strange_render_context_begin_frame(struct strange_render_context *context);
int strange_render_context_present(struct strange_render_context *context);
void strange_render_context_destroy(struct strange_render_context *context);

/*
  Write `num_chars` from `chars` to the screen at (x, y), where the origin is
  defined as the upper left corner of the window. The screen location is
  blanked using the padding character before any new characters are written.

  This function is provided for convenience. Copying larger regions of memory
  into the buffer directly will be faster than individual characters if you can
  do it.

  This function does no bounds checking at all, that is up to the caller.
*/
void write_to_buffer(struct ScreenBuffer *sbuffer, const char *chars,
                     int num_chars, int x, int y);
void write_string_to_buffer(struct ScreenBuffer *sbuffer, const char *text,
                            int x, int y);

#endif  // RENDERER_H_
