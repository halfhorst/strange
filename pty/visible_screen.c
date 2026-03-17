#include "visible_screen.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static size_t buffer_bytes(const struct ScreenBuffer *buffer) {
  return (size_t)buffer->w * (size_t)buffer->h *
         (size_t)buffer->character_width;
}

static void clear_row_range(struct ScreenBuffer *buffer, int y, int start_x,
                            int end_x) {
  if (buffer == NULL || buffer->buffer == NULL || y < 0 || y >= buffer->h ||
      start_x > end_x) {
    return;
  }

  if (start_x < 0) {
    start_x = 0;
  }
  if (end_x >= buffer->w) {
    end_x = buffer->w - 1;
  }

  for (int x = start_x; x <= end_x; ++x) {
    buffer->buffer[(y * buffer->w) + x] = SL_SPACE_CHAR;
  }
}

static void clear_screen_from_cursor(struct strange_visible_screen *screen) {
  clear_row_range(&screen->current, screen->cursor_y, screen->cursor_x,
                  screen->current.w - 1);
  for (int y = screen->cursor_y + 1; y < screen->current.h; ++y) {
    clear_row_range(&screen->current, y, 0, screen->current.w - 1);
  }
}

static void clear_screen_to_cursor(struct strange_visible_screen *screen) {
  for (int y = 0; y < screen->cursor_y; ++y) {
    clear_row_range(&screen->current, y, 0, screen->current.w - 1);
  }
  clear_row_range(&screen->current, screen->cursor_y, 0, screen->cursor_x);
}

static void scroll_up(struct strange_visible_screen *screen) {
  size_t row_bytes = (size_t)screen->current.w;

  if (screen->current.h <= 1) {
    clear_row_range(&screen->current, 0, 0, screen->current.w - 1);
    return;
  }

  memmove(screen->current.buffer, screen->current.buffer + row_bytes,
          row_bytes * (size_t)(screen->current.h - 1));
  clear_row_range(&screen->current, screen->current.h - 1, 0,
                  screen->current.w - 1);
}

static void clamp_cursor(struct strange_visible_screen *screen) {
  if (screen->cursor_x < 0) {
    screen->cursor_x = 0;
  }
  if (screen->cursor_y < 0) {
    screen->cursor_y = 0;
  }
  if (screen->cursor_x >= screen->current.w) {
    screen->cursor_x = screen->current.w - 1;
  }
  if (screen->cursor_y >= screen->current.h) {
    screen->cursor_y = screen->current.h - 1;
  }
}

static void save_cursor(struct strange_visible_screen *screen) {
  screen->saved_cursor_x = screen->cursor_x;
  screen->saved_cursor_y = screen->cursor_y;
  screen->saved_cursor_ready = 1;
}

static void restore_cursor(struct strange_visible_screen *screen) {
  if (!screen->saved_cursor_ready) {
    return;
  }

  screen->cursor_x = screen->saved_cursor_x;
  screen->cursor_y = screen->saved_cursor_y;
  clamp_cursor(screen);
}

static void line_feed(struct strange_visible_screen *screen) {
  if (screen->cursor_y >= screen->current.h - 1) {
    scroll_up(screen);
    screen->cursor_y = screen->current.h - 1;
    return;
  }

  screen->cursor_y++;
}

static void advance_cursor(struct strange_visible_screen *screen) {
  if (screen->cursor_x >= screen->current.w - 1) {
    screen->cursor_x = 0;
    line_feed(screen);
    return;
  }

  screen->cursor_x++;
}

static void put_byte(struct strange_visible_screen *screen, unsigned char byte) {
  screen->current.buffer[(screen->cursor_y * screen->current.w) +
                         screen->cursor_x] = (char)byte;
  advance_cursor(screen);
}

static int csi_param(const char *params, size_t length, size_t index,
                     int default_value) {
  size_t current_index = 0;
  int value = 0;
  int has_digits = 0;
  size_t start = 0;

  if (length > 0 && params[0] == '?') {
    start = 1;
  }

  if (start >= length && index == 0) {
    return default_value;
  }

  for (size_t i = start; i <= length; ++i) {
    if (i == length || params[i] == ';') {
      if (current_index == index) {
        return has_digits ? value : default_value;
      }
      current_index++;
      value = 0;
      has_digits = 0;
      continue;
    }
    if (params[i] >= '0' && params[i] <= '9') {
      value = (value * 10) + (params[i] - '0');
      has_digits = 1;
    }
  }

  return default_value;
}

static void handle_csi(struct strange_visible_screen *screen, char final) {
  const char *params = screen->csi_buffer;
  size_t length = screen->csi_length;
  int amount = 0;

  switch (final) {
  case 'A':
    amount = csi_param(params, length, 0, 1);
    screen->cursor_y -= amount;
    clamp_cursor(screen);
    return;
  case 'B':
    amount = csi_param(params, length, 0, 1);
    screen->cursor_y += amount;
    clamp_cursor(screen);
    return;
  case 'C':
    amount = csi_param(params, length, 0, 1);
    screen->cursor_x += amount;
    clamp_cursor(screen);
    return;
  case 'D':
    amount = csi_param(params, length, 0, 1);
    screen->cursor_x -= amount;
    clamp_cursor(screen);
    return;
  case 'G':
    screen->cursor_x = csi_param(params, length, 0, 1) - 1;
    clamp_cursor(screen);
    return;
  case 'H':
  case 'f':
    screen->cursor_y = csi_param(params, length, 0, 1) - 1;
    screen->cursor_x = csi_param(params, length, 1, 1) - 1;
    clamp_cursor(screen);
    return;
  case 'J':
    amount = csi_param(params, length, 0, 0);
    if (amount == 0) {
      clear_screen_from_cursor(screen);
    } else if (amount == 1) {
      clear_screen_to_cursor(screen);
    } else if (amount == 2) {
      strange_screen_buffer_clear(&screen->current);
    }
    return;
  case 'K':
    amount = csi_param(params, length, 0, 0);
    if (amount == 0) {
      clear_row_range(&screen->current, screen->cursor_y, screen->cursor_x,
                      screen->current.w - 1);
    } else if (amount == 1) {
      clear_row_range(&screen->current, screen->cursor_y, 0, screen->cursor_x);
    } else if (amount == 2) {
      clear_row_range(&screen->current, screen->cursor_y, 0,
                      screen->current.w - 1);
    }
    return;
  case 'h':
  case 'l':
    if (length > 0 && params[0] == '?' &&
        csi_param(params, length, 0, 0) == 25) {
      screen->cursor_visible = final == 'h';
    }
    return;
  case 'm':
    return;
  case 's':
    save_cursor(screen);
    return;
  case 'u':
    restore_cursor(screen);
    return;
  default:
    return;
  }
}

static void process_byte(struct strange_visible_screen *screen,
                         unsigned char byte) {
  switch (screen->parse_state) {
  case STRANGE_VISIBLE_SCREEN_TEXT:
    if (byte == '\033') {
      screen->parse_state = STRANGE_VISIBLE_SCREEN_ESCAPE;
      return;
    }
    if (byte == '\r') {
      screen->cursor_x = 0;
      return;
    }
    if (byte == '\n') {
      line_feed(screen);
      return;
    }
    if (byte == '\b') {
      if (screen->cursor_x > 0) {
        screen->cursor_x--;
      }
      return;
    }
    if (byte == '\t') {
      int next_stop = ((screen->cursor_x / 8) + 1) * 8;
      do {
        put_byte(screen, SL_SPACE_CHAR);
      } while (screen->cursor_x < next_stop && screen->cursor_x != 0);
      return;
    }
    if (byte >= 0x20 && byte != 0x7f) {
      put_byte(screen, byte);
    }
    return;
  case STRANGE_VISIBLE_SCREEN_ESCAPE:
    if (byte == '[') {
      screen->parse_state = STRANGE_VISIBLE_SCREEN_CSI;
      screen->csi_length = 0;
      return;
    }
    if (byte == ']') {
      screen->parse_state = STRANGE_VISIBLE_SCREEN_OSC;
      return;
    }
    screen->parse_state = STRANGE_VISIBLE_SCREEN_TEXT;
    if (byte == '7') {
      save_cursor(screen);
    } else if (byte == '8') {
      restore_cursor(screen);
    } else if (byte == 'D') {
      line_feed(screen);
    } else if (byte == 'E') {
      screen->cursor_x = 0;
      line_feed(screen);
    } else if (byte == 'c') {
      strange_screen_buffer_clear(&screen->current);
      screen->cursor_x = 0;
      screen->cursor_y = 0;
      screen->cursor_visible = 1;
    }
    return;
  case STRANGE_VISIBLE_SCREEN_CSI:
    if (byte >= 0x40 && byte <= 0x7e) {
      handle_csi(screen, (char)byte);
      screen->parse_state = STRANGE_VISIBLE_SCREEN_TEXT;
      screen->csi_length = 0;
      return;
    }
    if (screen->csi_length + 1 >= sizeof(screen->csi_buffer)) {
      screen->parse_state = STRANGE_VISIBLE_SCREEN_TEXT;
      screen->csi_length = 0;
      return;
    }
    screen->csi_buffer[screen->csi_length++] = (char)byte;
    screen->csi_buffer[screen->csi_length] = '\0';
    return;
  case STRANGE_VISIBLE_SCREEN_OSC:
    if (byte == '\a') {
      screen->parse_state = STRANGE_VISIBLE_SCREEN_TEXT;
    } else if (byte == '\033') {
      screen->parse_state = STRANGE_VISIBLE_SCREEN_OSC_ESC;
    }
    return;
  case STRANGE_VISIBLE_SCREEN_OSC_ESC:
    screen->parse_state =
        byte == '\\' ? STRANGE_VISIBLE_SCREEN_TEXT : STRANGE_VISIBLE_SCREEN_OSC;
    return;
  }
}

static int copy_buffer(struct ScreenBuffer *dest,
                       const struct ScreenBuffer *source) {
  if (dest == NULL || source == NULL || source->buffer == NULL ||
      source->character_width < 1) {
    errno = EINVAL;
    return -1;
  }

  if (strange_screen_buffer_init(dest, source->w, source->h,
                                 source->character_width) == -1) {
    return -1;
  }

  memcpy(dest->buffer, source->buffer, buffer_bytes(source));
  return 0;
}

static int write_all(int fd, const char *data, size_t length) {
  size_t written = 0;

  while (written < length) {
    ssize_t result = write(fd, data + written, length - written);
    if (result > 0) {
      written += (size_t)result;
      continue;
    }
    if (result == -1 && errno == EINTR) {
      continue;
    }
    return -1;
  }

  return 0;
}

static const struct ScreenBuffer *restored_buffer(
    const struct strange_visible_screen *screen, int *cursor_x, int *cursor_y,
    int *cursor_visible) {
  if (screen->snapshot_valid) {
    *cursor_x = screen->snapshot_cursor_x;
    *cursor_y = screen->snapshot_cursor_y;
    *cursor_visible = screen->snapshot_cursor_visible;
    return &screen->snapshot;
  }

  *cursor_x = screen->cursor_x;
  *cursor_y = screen->cursor_y;
  *cursor_visible = screen->cursor_visible;
  return &screen->current;
}

int strange_visible_screen_init(struct strange_visible_screen *screen, int w,
                                int h) {
  if (screen == NULL) {
    errno = EINVAL;
    return -1;
  }

  memset(screen, 0, sizeof(*screen));
  if (strange_screen_buffer_init(&screen->current, w, h, 1) == -1) {
    return -1;
  }

  screen->cursor_visible = 1;
  return 0;
}

void strange_visible_screen_destroy(struct strange_visible_screen *screen) {
  if (screen == NULL) {
    return;
  }

  strange_screen_buffer_free(&screen->current);
  strange_screen_buffer_free(&screen->snapshot);
  memset(screen, 0, sizeof(*screen));
}

int strange_visible_screen_write(struct strange_visible_screen *screen,
                                 const char *data, size_t length) {
  if (screen == NULL || data == NULL || screen->current.buffer == NULL) {
    errno = EINVAL;
    return -1;
  }

  for (size_t i = 0; i < length; ++i) {
    process_byte(screen, (unsigned char)data[i]);
  }

  return 0;
}

int strange_visible_screen_capture_snapshot(
    struct strange_visible_screen *screen) {
  struct ScreenBuffer snapshot = {0};

  if (screen == NULL || screen->current.buffer == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (copy_buffer(&snapshot, &screen->current) == -1) {
    return -1;
  }

  strange_screen_buffer_free(&screen->snapshot);
  screen->snapshot = snapshot;
  screen->snapshot_cursor_x = screen->cursor_x;
  screen->snapshot_cursor_y = screen->cursor_y;
  screen->snapshot_cursor_visible = screen->cursor_visible;
  screen->snapshot_valid = 1;
  return 0;
}

void strange_visible_screen_discard_snapshot(
    struct strange_visible_screen *screen) {
  if (screen == NULL) {
    return;
  }

  strange_screen_buffer_free(&screen->snapshot);
  screen->snapshot_valid = 0;
}

int strange_visible_screen_resize(struct strange_visible_screen *screen, int w,
                                  int h, int clear_current,
                                  int discard_snapshot) {
  struct ScreenBuffer resized = {0};
  int copy_width = 0;
  int copy_height = 0;

  if (screen == NULL || screen->current.buffer == NULL) {
    errno = EINVAL;
    return -1;
  }
  if (screen->current.w == w && screen->current.h == h) {
    return 0;
  }

  if (strange_screen_buffer_init(&resized, w, h, 1) == -1) {
    return -1;
  }

  if (!clear_current) {
    copy_width = screen->current.w < w ? screen->current.w : w;
    copy_height = screen->current.h < h ? screen->current.h : h;

    for (int row = 0; row < copy_height; ++row) {
      memcpy(resized.buffer + (row * resized.w),
             screen->current.buffer + (row * screen->current.w),
             (size_t)copy_width);
    }
  }

  strange_screen_buffer_free(&screen->current);
  screen->current = resized;
  if (clear_current) {
    screen->cursor_x = 0;
    screen->cursor_y = 0;
    screen->cursor_visible = 1;
  } else {
    clamp_cursor(screen);
  }

  if (discard_snapshot) {
    strange_visible_screen_discard_snapshot(screen);
  }

  return 0;
}

int strange_visible_screen_restore(struct strange_visible_screen *screen,
                                   FILE *stream) {
  const struct ScreenBuffer *buffer = NULL;
  int cursor_x = 0;
  int cursor_y = 0;
  int cursor_visible = 1;

  if (screen == NULL || stream == NULL || screen->current.buffer == NULL) {
    errno = EINVAL;
    return -1;
  }

  buffer = restored_buffer(screen, &cursor_x, &cursor_y, &cursor_visible);

  if (fprintf(stream, "\033[2J\033[H") < 0) {
    return -1;
  }

  for (int row = 0; row < buffer->h; ++row) {
    size_t offset = (size_t)row * (size_t)buffer->w;

    if (fwrite(buffer->buffer + offset, 1, (size_t)buffer->w, stream) !=
        (size_t)buffer->w) {
      return -1;
    }
    if (fputc('\n', stream) == EOF) {
      return -1;
    }
  }

  if (fprintf(stream, "\033[%d;%dH\033[?25%c", cursor_y + 1, cursor_x + 1,
              cursor_visible ? 'h' : 'l') < 0) {
    return -1;
  }

  return fflush(stream) == EOF ? -1 : 0;
}

int strange_visible_screen_restore_to_fd(struct strange_visible_screen *screen,
                                         int fd) {
  const struct ScreenBuffer *buffer = NULL;
  int cursor_x = 0;
  int cursor_y = 0;
  int cursor_visible = 1;
  char cursor_sequence[32];
  int cursor_length = 0;

  if (screen == NULL || fd < 0 || screen->current.buffer == NULL) {
    errno = EINVAL;
    return -1;
  }

  buffer = restored_buffer(screen, &cursor_x, &cursor_y, &cursor_visible);

  if (write_all(fd, "\033[2J\033[H", strlen("\033[2J\033[H")) == -1) {
    return -1;
  }

  for (int row = 0; row < buffer->h; ++row) {
    size_t offset = (size_t)row * (size_t)buffer->w;

    if (write_all(fd, buffer->buffer + offset, (size_t)buffer->w) == -1 ||
        write_all(fd, "\n", 1) == -1) {
      return -1;
    }
  }

  cursor_length = snprintf(cursor_sequence, sizeof(cursor_sequence),
                           "\033[%d;%dH\033[?25%c", cursor_y + 1, cursor_x + 1,
                           cursor_visible ? 'h' : 'l');
  if (cursor_length < 0 ||
      (size_t)cursor_length >= sizeof(cursor_sequence) ||
      write_all(fd, cursor_sequence, (size_t)cursor_length) == -1) {
    return -1;
  }

  return 0;
}
