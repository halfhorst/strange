#ifndef RENDERER_H_
#define RENDERER_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// The padding character used by the renderer. Non-printing C0 control seems to
// have inconsistent behavior. I trust \0 to never represent a character. The
// dirty trick is printing past it when rendering.
#define STRANGE_PAD_CHAR 0x0

// The space character used by the renderer for creating an empty screen
#define STRANGE_SPACE_CHAR 0x20

struct strange_screen_buffer {
  int w;
  int h;
  char *buffer;
  int character_width;  // the number of characters reserved for each (x, y)
};

struct strange_render_context {
  struct strange_screen_buffer buffer;
  struct strange_screen_buffer presented;  // what the terminal currently shows
  int terminal_fd;
  FILE *stream;
  unsigned long frame_count;
};

int strange_terminal_size(int fd, int *w, int *h);
int strange_screen_buffer_init(struct strange_screen_buffer *buffer, int w,
                               int h, int character_width);
int strange_screen_buffer_resize(struct strange_screen_buffer *buffer, int w,
                                 int h);
void strange_screen_buffer_free(struct strange_screen_buffer *buffer);
void strange_screen_buffer_clear(struct strange_screen_buffer *buffer);
int strange_render_context_init(struct strange_render_context *context,
                                int terminal_fd, FILE *stream,
                                int character_width);
int strange_render_context_refresh_size(struct strange_render_context *context);
void strange_render_context_begin_frame(struct strange_render_context *context);
/*
  Send the frame in `buffer` to the terminal. Only cells that differ from the
  previous frame are written, unless the size changed since then.
*/
int strange_render_context_present(struct strange_render_context *context);
void strange_render_context_destroy(struct strange_render_context *context);

/*
  Write `num_chars` from `chars` to the screen at (x, y), where the origin is
  defined as the upper left corner of the window. The screen location is
  blanked using the padding character before any new characters are written.

  This function is provided for convenience. Copying larger regions of memory
  into the buffer directly will be faster than individual characters if you can
  do it.

  Writes that fall outside the buffer, or that are longer than a cell, are
  ignored.
*/
void strange_screen_buffer_write(struct strange_screen_buffer *buffer,
                                 const char *chars, int num_chars, int x,
                                 int y);
void strange_screen_buffer_write_string(struct strange_screen_buffer *buffer,
                                        const char *text, int x, int y);

#ifdef __cplusplus
}
#endif

#endif  // RENDERER_H_
