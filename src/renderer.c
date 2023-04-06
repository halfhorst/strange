#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// #include <signal.h>
// #include <termio.h>
// #include <time.h>
// #include <unistd.h>
#include <math.h>

#include "tty.h"
#include "renderer.h"
#include "metrics.h"
#include "slsignal.h"
#include "timing.h"

/* Initialize a screen buffer on the stack. Exits the program on failure. */
struct ScreenBuffer *init_screen_buffer(int w, int h, int character_width);


void free_screen_buffer(struct ScreenBuffer *buffer);

/*
  Allocate a character pattern on the heap. The pattern is the SL_SPACE_CHAR
  followed by enoug SL_PADDING_CHAR to fill out the screen buffer's character
  width. This pattern is allocated once for use when clearing the screen.
*/
char *generate_clear_pattern(struct ScreenBuffer *sbuffer);


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


int render(void (*ss_init)(struct ScreenBuffer *sbuffer),
           bool (*ss_update)(struct ScreenBuffer *sbuffer,
                             unsigned long frame_count),
           void (*ss_cleanup)(void),
           int character_width,
           int delay) {

  if (character_width < 1) {
    return EXIT_FAILURE;
  }

  int w, h;
  if (get_window_size(&w, &h)) {
    return EXIT_FAILURE;
  }

  register_cleanup(ss_cleanup);
  // scene_cleanup = ss_cleanup;
  // signal(SIGINT, signal_cleanup);
  // signal(SIGKILL, signal_cleanup);

  if (prepare_tty()) {
    return EXIT_FAILURE;
  }

  sbuffer = init_screen_buffer(w, h, character_width);
  char *clear_pattern = generate_clear_pattern(sbuffer);
  ss_init(sbuffer);

  unsigned long frame_count = 0;
  int prev_w, prev_h;
  for (;;) {
    prev_w = w;
    prev_h = h;
    if (get_window_size(&w, &h) != 0) {
      ss_cleanup();
      free_screen_buffer(sbuffer);
      restore_tty();
      return EXIT_FAILURE;
    }

// #ifndef DEBUG
    if ((w != prev_w) || (h != prev_h)) {
      free_screen_buffer(sbuffer);
      sbuffer = init_screen_buffer(w, h, character_width);
      clear_tty(); // re-sizing introduces artifacts
    } else {
      clear_screen_buffer(sbuffer, clear_pattern);
    }
// #endif  // DEBUG


    start_clock();
    if (!ss_update(sbuffer, frame_count)) {
      break;
    }
    // long duration_micro_seconds = end_clock();

    // TODO: Calculate smoothed FPS and overwrite the screen buffer
    // long fps = calculate_smoothed_fps(duration_micro_seconds);
    // draw_fps(sbuffer, fps);

// #ifndef DEBUG
    print_to_tty(sbuffer);
// #endif  // DEBUG

    frame_count++;

    // note this is susceptible to pre-emption by the CPU :()
    // usleep(fmax(0, delay - duration_micro_seconds));
  }

  ss_cleanup();
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
  the buffer's dimensions. If these conditions are violated, nothing is
  written.
*/
void write_to_buffer(struct ScreenBuffer *sbuffer, char *chars, int num_chars,
                     int x, int y) {
  int index = sbuffer->character_width * ((sbuffer->w * y) + x);

  for (int i = 0; i < sbuffer->character_width; i++) {
    sbuffer->buffer[index + i] = SL_PAD_CHAR;
  }
  memcpy(sbuffer->buffer + index, chars, num_chars);
}

char *generate_clear_pattern(struct ScreenBuffer *sbuffer) {
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

// void print_screen_buffer(struct ScreenBuffer *sbuffer) {
//   // Reposition cursor to the upper left. Rows are 1-indexed
//   printf("\033[1;0H");

//   for (int i = 0; i < sbuffer->h; i++) {
//     // fwrite will write past \0, which is great because it's my
//     // foolproof padding char
//     fwrite(sbuffer->buffer + (i * sbuffer->w * sbuffer->character_width),
//            1, (sbuffer->w * sbuffer->character_width), stdout);
//     printf("\n");
//   }
// }

// void cleanup() {
//   ss_cleanup();
//   free_screen_buffer(sbuffer);
//   restore_tty();
// }
