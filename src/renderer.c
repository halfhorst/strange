#include "renderer.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

// Unchanged cells between two changed ones are rewritten when the gap is
// shorter than this, which costs less than another cursor move.
#define STRANGE_RENDER_RUN_MAX_GAP 4

static size_t screen_buffer_bytes(const struct strange_screen_buffer *buffer) {
  return (size_t)buffer->w * (size_t)buffer->h *
         (size_t)buffer->character_width;
}

static int initialize_screen_buffer(struct strange_screen_buffer *buffer, int w,
                                    int h, int character_width) {
  if (buffer == NULL || character_width < 1 || w < 1 || h < 1) {
    errno = EINVAL;
    return -1;
  }

  buffer->buffer = malloc((size_t)w * (size_t)h * (size_t)character_width);
  if (buffer->buffer == NULL) {
    return -1;
  }

  buffer->w = w;
  buffer->h = h;
  buffer->character_width = character_width;
  strange_screen_buffer_clear(buffer);
  return 0;
}

int strange_terminal_size(int fd, int *w, int *h) {
  struct winsize ws;

  if (ioctl(fd, TIOCGWINSZ, &ws) == -1) {
    return -1;
  }
  if (ws.ws_col < 1 || ws.ws_row < 1) {
    errno = ERANGE;
    return -1;
  }

  *w = ws.ws_col;
  *h = ws.ws_row;
  return 0;
}

int strange_screen_buffer_init(struct strange_screen_buffer *buffer, int w,
                               int h, int character_width) {
  if (buffer == NULL) {
    errno = EINVAL;
    return -1;
  }

  buffer->w = 0;
  buffer->h = 0;
  buffer->character_width = 0;
  buffer->buffer = NULL;
  return initialize_screen_buffer(buffer, w, h, character_width);
}

int strange_screen_buffer_resize(struct strange_screen_buffer *buffer, int w,
                                 int h) {
  struct strange_screen_buffer resized = {0};

  if (buffer == NULL || buffer->character_width < 1) {
    errno = EINVAL;
    return -1;
  }

  if (buffer->w == w && buffer->h == h && buffer->buffer != NULL) {
    strange_screen_buffer_clear(buffer);
    return 0;
  }

  if (initialize_screen_buffer(&resized, w, h, buffer->character_width) == -1) {
    return -1;
  }

  strange_screen_buffer_free(buffer);
  *buffer = resized;
  return 0;
}

void strange_screen_buffer_free(struct strange_screen_buffer *buffer) {
  if (buffer == NULL) {
    return;
  }

  free(buffer->buffer);
  buffer->buffer = NULL;
  buffer->w = 0;
  buffer->h = 0;
  buffer->character_width = 0;
}

void strange_screen_buffer_clear(struct strange_screen_buffer *buffer) {
  if (buffer == NULL || buffer->buffer == NULL || buffer->character_width < 1) {
    return;
  }

  memset(buffer->buffer, STRANGE_PAD_CHAR, screen_buffer_bytes(buffer));

  for (size_t index = 0; index < screen_buffer_bytes(buffer);
       index += (size_t)buffer->character_width) {
    buffer->buffer[index] = STRANGE_SPACE_CHAR;
  }
}

int strange_render_context_init(struct strange_render_context *context,
                                int terminal_fd, FILE *stream,
                                int character_width) {
  int w = 0;
  int h = 0;

  if (context == NULL || stream == NULL || character_width < 1) {
    errno = EINVAL;
    return -1;
  }

  memset(context, 0, sizeof(*context));
  if (strange_terminal_size(terminal_fd, &w, &h) == -1) {
    return -1;
  }
  if (strange_screen_buffer_init(&context->buffer, w, h, character_width) ==
      -1) {
    return -1;
  }

  context->terminal_fd = terminal_fd;
  context->stream = stream;
  context->frame_count = 0;
  return 0;
}

int strange_render_context_refresh_size(struct strange_render_context *context) {
  int w = 0;
  int h = 0;

  if (context == NULL) {
    errno = EINVAL;
    return -1;
  }
  if (strange_terminal_size(context->terminal_fd, &w, &h) == -1) {
    return -1;
  }

  return strange_screen_buffer_resize(&context->buffer, w, h);
}

void strange_render_context_begin_frame(struct strange_render_context *context) {
  if (context == NULL) {
    return;
  }

  strange_screen_buffer_clear(&context->buffer);
}

static int write_cells(FILE *stream, const struct strange_screen_buffer *buffer,
                       int row, int start, int end) {
  size_t cell_bytes = (size_t)buffer->character_width;
  size_t offset = ((size_t)row * (size_t)buffer->w + (size_t)start) * cell_bytes;
  size_t length = (size_t)(end - start) * cell_bytes;

  if (fprintf(stream, "\033[%d;%dH", row + 1, start + 1) < 0 ||
      fwrite(buffer->buffer + offset, 1, length, stream) != length) {
    return -1;
  }

  return 0;
}

static int cell_changed(const struct strange_screen_buffer *buffer,
                        const struct strange_screen_buffer *presented, int row,
                        int x) {
  size_t cell_bytes = (size_t)buffer->character_width;
  size_t offset = ((size_t)row * (size_t)buffer->w + (size_t)x) * cell_bytes;

  return memcmp(buffer->buffer + offset, presented->buffer + offset,
                cell_bytes) != 0;
}

static int write_changed_cells(FILE *stream,
                               const struct strange_screen_buffer *buffer,
                               const struct strange_screen_buffer *presented,
                               int row) {
  int x = 0;

  while (x < buffer->w) {
    int end = 0;

    if (!cell_changed(buffer, presented, row, x)) {
      x++;
      continue;
    }

    end = x + 1;
    for (int next = end;
         next < buffer->w && next - end < STRANGE_RENDER_RUN_MAX_GAP; ++next) {
      if (cell_changed(buffer, presented, row, next)) {
        end = next + 1;
      }
    }

    if (write_cells(stream, buffer, row, x, end) == -1) {
      return -1;
    }
    x = end;
  }

  return 0;
}

int strange_render_context_present(struct strange_render_context *context) {
  const struct strange_screen_buffer *buffer = NULL;
  struct strange_screen_buffer *presented = NULL;
  int redraw_everything = 0;

  if (context == NULL || context->stream == NULL || context->buffer.buffer == NULL) {
    errno = EINVAL;
    return -1;
  }

  buffer = &context->buffer;
  presented = &context->presented;
  if (presented->buffer == NULL || presented->w != buffer->w ||
      presented->h != buffer->h ||
      presented->character_width != buffer->character_width) {
    strange_screen_buffer_free(presented);
    if (strange_screen_buffer_init(presented, buffer->w, buffer->h,
                                   buffer->character_width) == -1) {
      return -1;
    }
    redraw_everything = 1;
  }

  for (int row = 0; row < buffer->h; ++row) {
    int result = redraw_everything
                     ? write_cells(context->stream, buffer, row, 0, buffer->w)
                     : write_changed_cells(context->stream, buffer, presented,
                                           row);
    if (result == -1) {
      return -1;
    }
  }

  memcpy(presented->buffer, buffer->buffer, screen_buffer_bytes(buffer));

  if (fflush(context->stream) == EOF) {
    return -1;
  }

  context->frame_count++;
  return 0;
}

void strange_render_context_destroy(struct strange_render_context *context) {
  if (context == NULL) {
    return;
  }

  strange_screen_buffer_free(&context->buffer);
  strange_screen_buffer_free(&context->presented);
  context->stream = NULL;
  context->terminal_fd = -1;
  context->frame_count = 0;
}

void strange_screen_buffer_write(struct strange_screen_buffer *buffer,
                                 const char *chars, int num_chars, int x,
                                 int y) {
  if (buffer == NULL || buffer->buffer == NULL || chars == NULL ||
      num_chars < 0 || num_chars > buffer->character_width || x < 0 || y < 0 ||
      x >= buffer->w || y >= buffer->h) {
    return;
  }

  int index = buffer->character_width * ((buffer->w * y) + x);
  memset(buffer->buffer + index, STRANGE_PAD_CHAR, buffer->character_width);
  memcpy(buffer->buffer + index, chars, (size_t)num_chars);
}

void strange_screen_buffer_write_string(struct strange_screen_buffer *buffer,
                                        const char *text, int x, int y) {
  size_t remaining_width = 0;

  if (buffer == NULL || text == NULL || x < 0 || y < 0 || x >= buffer->w ||
      y >= buffer->h) {
    return;
  }

  remaining_width = (size_t)(buffer->w - x);
  for (size_t index = 0; text[index] != '\0' && index < remaining_width;
       ++index) {
    strange_screen_buffer_write(buffer, text + index, 1, x + (int)index, y);
  }
}
