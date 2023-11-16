#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "renderer.h"
#include "cleanup.h"
#include "timing.h"
#include "tty.h"
#include "screensaver.h"

/* Initialize a screen buffer in the heap. Exits the program on failure. */
struct ScreenBuffer *init_screen_buffer(int w, int h, int character_width);

/* Free a screen buffer's memory */
void free_screen_buffer(struct ScreenBuffer *buffer);

/*
  Allocate a character pattern on the heap. The pattern is the SL_SPACE_CHAR
  followed by enoug SL_PADDING_CHAR to fill out the screen buffer's character
  width. This pattern is allocated once for use when clearing the screen.
*/
char *init_clear_pattern(struct ScreenBuffer *sbuffer);

/* Set the buffer to a blank screen. */
void clear_screen_buffer(struct ScreenBuffer *buffer, char *pattern);

/*
  Reposition the cursor to the top left of the terminal and print the screen
  buffer to stdout.
*/
void print_screen_buffer(struct ScreenBuffer *buffer);

/*
  The signal handling function. Calls the demo's cleanup function and restores
  the tty.
*/

// The cleanup method. Combines the user-provided scene cleanup with tty and memory
// handling.
void cleanup();

static struct ScreenBuffer *buffer;

int render(struct ScreenSaver screensaver) {
int window_width, window_height;
  if (get_window_size(&window_width, &window_height)) {
    return EXIT_FAILURE;
  }

  buffer = init_screen_buffer(window_width, window_height, screensaver.character_width);
  char *clear_pattern = init_clear_pattern(buffer);
  init_timer();

  uint64_t previous_time = get_time_ms();
  int prev_window_width, prev_window_height;
  for (;;) {
    // check window size
    prev_window_width = window_width;
    prev_window_height = window_height;
    if (get_window_size(&window_width, &window_height) != 0) {
      screensaver.cleanup();
      free_screen_buffer(buffer);
      restore_tty();
      return EXIT_FAILURE;
    }

    // Re-size if necessary
    if ((window_width != prev_window_width) || (window_height != prev_window_height)) {
      free_screen_buffer(buffer);
      buffer = init_screen_buffer(window_width, window_height, screensaver.character_width);
      clear_tty(); // re-sizing introduces artifacts
    } else {
      clear_screen_buffer(buffer, clear_pattern);
    }

    uint64_t current_time = get_time_ms();
    uint64_t elapsed_time = current_time - previous_time;
    previous_time = current_time;
    if (!screensaver.update(buffer, get_time_ms(), elapsed_time)) {
      break;
    }

    print_to_tty(buffer);
  }

  screensaver.cleanup();
  free_screen_buffer(buffer);
  restore_tty();

  return EXIT_SUCCESS;
}

struct ScreenBuffer *init_screen_buffer(int w, int h, int character_width) {
  struct ScreenBuffer *buffer = malloc(sizeof(struct ScreenBuffer));
  if (buffer == NULL) {
    fprintf(stderr, "Failed to allocate screen buffer");
    exit(EXIT_FAILURE);
  }
  buffer->w = w;
  buffer->h = h;
  buffer->character_width = character_width;
  buffer->buffer = malloc(sizeof(char) * character_width * w * h);
  if (buffer->buffer == NULL) {
    exit(EXIT_FAILURE);
  }
  return buffer;
}

void free_screen_buffer(struct ScreenBuffer *buffer) {
  free(buffer->buffer);
  free(buffer);
}

/*
  Write bytes to the (x, y) coordinate specified. The origin is defined as
  the upper left corner of the screen. The number of bytes cannot exceed
  the character_width of the screen buffer, and `x` annd `y` cannot exceed
  the buffer's dimensions. If these conditions are violated the result is 
  a no-op.
*/
void write_to_buffer(struct ScreenBuffer *buffer, char *chars, 
                     uint32_t num_chars, uint16_t x, uint16_t y) {
  int index = buffer->character_width * ((buffer->w * y) + x);

  for (int i = 0; i < buffer->character_width; i++) {
    buffer->buffer[index + i] = SL_PAD_CHAR;
  }
  memcpy(buffer->buffer + index, chars, num_chars);
}

char *init_clear_pattern(struct ScreenBuffer *buffer) {
  char *pattern = malloc(sizeof(char) * buffer->character_width);
  pattern[0] = SL_SPACE_CHAR;
  for (int i = 1; i < buffer->character_width; i++) {
    pattern[i] = SL_PAD_CHAR;
  }
  return pattern;
}

void clear_screen_buffer(struct ScreenBuffer *buffer, char *pattern) {
  for (int i = 0;
        i < (buffer->w * buffer->h * buffer->character_width);
          i += buffer->character_width) {
    memcpy(buffer->buffer + i, pattern, buffer->character_width);
  }
}

void draw_fps(struct ScreenBuffer *buffer, float fps) {
    uint16_t rounded_fps = round(fps);
    uint8_t x_chars_required = 9;
    uint8_t y_chars_required = 5;

    if (buffer->w < x_chars_required || buffer->h < y_chars_required) {
        return;
    }

    char formatted_fps[20];
    snprintf(formatted_fps, sizeof(formatted_fps), " | %.3i | ", rounded_fps);

    // write_to_buffer(buffer, " ───── ", 7, 0, 1);
    write_to_buffer(buffer, formatted_fps, 9, 0, 2);
    // write_to_buffer(buffer, " ───── ", 7, 0, 3);
}
