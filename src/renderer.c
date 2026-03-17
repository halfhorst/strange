#include "renderer.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static size_t screen_buffer_bytes(const struct ScreenBuffer *buffer) {
  return (size_t)buffer->w * (size_t)buffer->h *
         (size_t)buffer->character_width;
}

static int initialize_screen_buffer(struct ScreenBuffer *buffer, int w, int h,
                                    int character_width) {
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

int strange_get_terminal_size(int fd, int *w, int *h) {
  struct winsize ws;

  if (ioctl(fd, TIOCGWINSZ, &ws) == -1) {
    return -1;
  }
  if (ws.ws_col < 2 || ws.ws_row < 2) {
    errno = ERANGE;
    return -1;
  }

  *w = ws.ws_col - 1;
  *h = ws.ws_row - 1;
  return 0;
}

int strange_screen_buffer_init(struct ScreenBuffer *buffer, int w, int h,
                               int character_width) {
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

int strange_screen_buffer_resize(struct ScreenBuffer *buffer, int w, int h) {
  struct ScreenBuffer resized = {0};

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

void strange_screen_buffer_free(struct ScreenBuffer *buffer) {
  if (buffer == NULL) {
    return;
  }

  free(buffer->buffer);
  buffer->buffer = NULL;
  buffer->w = 0;
  buffer->h = 0;
  buffer->character_width = 0;
}

void strange_screen_buffer_clear(struct ScreenBuffer *buffer) {
  if (buffer == NULL || buffer->buffer == NULL || buffer->character_width < 1) {
    return;
  }

  memset(buffer->buffer, SL_PAD_CHAR, screen_buffer_bytes(buffer));

  for (size_t index = 0; index < screen_buffer_bytes(buffer);
       index += (size_t)buffer->character_width) {
    buffer->buffer[index] = SL_SPACE_CHAR;
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
  if (strange_get_terminal_size(terminal_fd, &w, &h) == -1) {
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
  if (strange_get_terminal_size(context->terminal_fd, &w, &h) == -1) {
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

int strange_render_context_present(struct strange_render_context *context) {
  if (context == NULL || context->stream == NULL || context->buffer.buffer == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (fprintf(context->stream, "\033[H") < 0) {
    return -1;
  }

  for (int row = 0; row < context->buffer.h; ++row) {
    size_t offset = (size_t)row * (size_t)context->buffer.w *
                    (size_t)context->buffer.character_width;
    size_t row_bytes = (size_t)context->buffer.w *
                       (size_t)context->buffer.character_width;

    if (fwrite(context->buffer.buffer + offset, 1, row_bytes, context->stream) !=
        row_bytes) {
      return -1;
    }
    if (fputc('\n', context->stream) == EOF) {
      return -1;
    }
  }

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
  context->stream = NULL;
  context->terminal_fd = -1;
  context->frame_count = 0;
}

void write_to_buffer(struct ScreenBuffer *sbuffer, const char *chars,
                     int num_chars, int x, int y) {
  if (sbuffer == NULL || sbuffer->buffer == NULL || chars == NULL ||
      num_chars < 0 || num_chars > sbuffer->character_width || x < 0 || y < 0 ||
      x >= sbuffer->w || y >= sbuffer->h) {
    return;
  }

  int index = sbuffer->character_width * ((sbuffer->w * y) + x);
  memset(sbuffer->buffer + index, SL_PAD_CHAR, sbuffer->character_width);
  memcpy(sbuffer->buffer + index, chars, (size_t)num_chars);
}

void write_string_to_buffer(struct ScreenBuffer *sbuffer, const char *text,
                            int x, int y) {
  size_t remaining_width = 0;

  if (sbuffer == NULL || text == NULL || x < 0 || y < 0 || x >= sbuffer->w ||
      y >= sbuffer->h) {
    return;
  }

  remaining_width = (size_t)(sbuffer->w - x);
  for (size_t index = 0; text[index] != '\0' && index < remaining_width;
       ++index) {
    write_to_buffer(sbuffer, text + index, 1, x + (int)index, y);
  }
}
