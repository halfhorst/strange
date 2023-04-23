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

// The render buffer
static struct ScreenBuffer *sbuffer;


// // global tty settings
// static struct termios render_term, restore_term;

  // // global cleanup function, assigned the demo's cleanup so the signal handler
// // can call it
// static void (*scene_cleanup)(void);


int render(struct ScreenSaver screensaver) {

  // if (character_width < 1) {
  //   return EXIT_FAILURE;
  // }

  int window_width, window_height;
  if (get_window_size(&window_width, &window_height)) {
    return EXIT_FAILURE;
  }

  register_cleanup(screensaver.cleanup);

  if (prepare_tty()) {
    return EXIT_FAILURE;
  }

  sbuffer = init_screen_buffer(window_width, window_height, screensaver.character_width);
  char *clear_pattern = init_clear_pattern(sbuffer);
  screensaver.init(sbuffer);

  init_timer();
  unsigned long frame_count = 0;
  int prev_window_width, prev_window_height;
  for (;;) {
    // check window size
    prev_window_width = window_width;
    prev_window_height = window_height;
    if (get_window_size(&window_width, &window_height) != 0) {
      screensaver.cleanup();
      free_screen_buffer(sbuffer);
      restore_tty();
      return EXIT_FAILURE;
    }

    // Re-size if necessary
    if ((window_width != prev_window_width) || (window_height != prev_window_height)) {
      free_screen_buffer(sbuffer);
      sbuffer = init_screen_buffer(window_width, window_height, screensaver.character_width);
      clear_tty(); // re-sizing introduces artifacts
    } else {
      clear_screen_buffer(sbuffer, clear_pattern);
    }


// #endif  // DEBUG
    // start_frame_measurement();
    uint64_t before = get_time_ms();
    if (!screensaver.update(sbuffer)) {
      break;
    }
    uint64_t after = get_time_ms();
    uint64_t dt = after - before;
    log_frame_time_ms(dt);
    // end_frame_measurement();
    // long duration_micro_seconds = end_clock();

    // TODO: Calculate smoothed FPS and overwrite the screen buffer
    // long fps = calculate_smoothed_fps(duration_micro_seconds);
    // draw_fps(sbuffer, fps);

// #ifndef DEBUG
    print_to_tty(sbuffer);
// #endif  // DEBUG

    frame_count++;
  }

  screensaver.cleanup();
  free_screen_buffer(sbuffer);
  restore_tty();

  return EXIT_SUCCESS;
}

struct ScreenBuffer *init_screen_buffer(int w, int h, int character_width) {
  struct ScreenBuffer *sbuffer = malloc(sizeof(struct ScreenBuffer));
  if (sbuffer == NULL) {
    fprintf(stderr, "Failed to allocate screen buffer");
    exit(EXIT_FAILURE);
  }
  sbuffer->w = w;
  sbuffer->h = h;
  sbuffer->character_width = character_width;
  sbuffer->buffer = malloc(sizeof(char) * character_width * w * h);
  if (sbuffer->buffer == NULL) {
    exit(EXIT_FAILURE);
  }
  return sbuffer;
}

void free_screen_buffer(struct ScreenBuffer *sbuffer) {
  free(sbuffer->buffer);
  free(sbuffer);
}

/*
  Write bytes to the (x, y) coordinate specified. The origin is defined as
  the upper left corner of the screen. The number of bytes cannot exceed
  the character_width of the screen buffer, and `x` annd `y` cannot exceed
  the buffer's dimensions. If these conditions are violated the result is 
  a no-op.
*/
void write_to_buffer(struct ScreenBuffer *sbuffer, char *chars, int num_chars,
                     int x, int y) {
  int index = sbuffer->character_width * ((sbuffer->w * y) + x);

  for (int i = 0; i < sbuffer->character_width; i++) {
    sbuffer->buffer[index + i] = SL_PAD_CHAR;
  }
  memcpy(sbuffer->buffer + index, chars, num_chars);
}


// TODO: Can I just do a memset?
// No, since I support multi-width characters
// pattern was more correct. I am only creating a pattern,
// then stamping it over and over again. in a for loop
// Can I do something better? Can't buffer swap because
// the cleared buffer will not remain clear
char *init_clear_pattern(struct ScreenBuffer *sbuffer) {
  char *pattern = malloc(sizeof(char) * sbuffer->character_width);
  pattern[0] = SL_SPACE_CHAR;
  for (int i = 1; i < sbuffer->character_width; i++) {
    pattern[i] = SL_PAD_CHAR;
  }
  return pattern;
}

void clear_screen_buffer(struct ScreenBuffer *sbuffer, char *pattern) {
  for (int i = 0;
        i < (sbuffer->w * sbuffer->h * sbuffer->character_width);
          i += sbuffer->character_width) {
    memcpy(sbuffer->buffer + i, pattern, sbuffer->character_width);
  }
}

void draw_fps(struct ScreenBuffer *buffer, float fps) {
    uint16_t rounded_fps = round(fps);
    int x_chars_required = 9;
    int y_chars_required = 5;

    if (buffer->w < x_chars_required || buffer->h < y_chars_required) {
        return;
    }

    char formatted_fps[20];
    snprintf(formatted_fps, sizeof(formatted_fps), " | %.3i | ", rounded_fps);

    // write_to_buffer(buffer, " ───── ", 7, 0, 1);
    write_to_buffer(buffer, formatted_fps, 9, 0, 2);
    // write_to_buffer(buffer, " ───── ", 7, 0, 3);
}
