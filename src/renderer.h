/*
  The strangeland rendering "engine."

  This takes the form of a single function that performs the actual terminal
  rendering. It is supplied with three functions that define the demo to be
  rendered

  void ss_init(screen_buffer)
    An initialization function run once before rendering. Generally, allocate
    what you need here to global state.

  bool ss_update(screen_buffer, frame_count)
    The update function, run once each frame followed by a draw to the
    terminal. The current frame count is passed in as an argument, and the
    screenbuffer contains the terminals current dimensions, which may have
    changed. The function returns a boolean that dictates whether rendering
    continues or not. This is where you modify the screen buffer for rendering.

  void ss_cleanup(void)
    A cleanup function, run once after rendering stops or on SIGINT or SIGKILL.

  The rendering lifecycle is simple. Initially the window size is collected,
  signal handlers are allocated, and the tty is prepared. This preparation
  entails preparing to termios structures, one with current tty settings and
  the other with modifications. Then the screen buffer is allocated and ss_init
  is called.

  Next, rendering begins in a loop. The window is checked for resizing and
  a new buffer is allocated if this has occurred. Then the update function is
  called, the screen buffer is printed to the terminal, and the process sleeps
  until a duration is satisfied.

  On SIGINT, SIGKILL or ss_update returning false, cleanup takes place.
*/
#include <stdbool.h>

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

/*
  The main render loop. `ss_init`, `ss_update`, and `ss_cleanup` should be
  provided to do initialization, screen buffering, and cleanup.

  `character_width` is the number of characters to allot for each visible
  character on the screen. Fixed-width characters are important for constant
  time lookup, but support for UTF-8 is important as well. We let each demo
  decide what it wants, but it must be fixed.

  `delay` is a duration in ms to wait after each update.
*/
int render(void (*ss_init)(struct ScreenBuffer *buffer),
           bool (*ss_update)(struct ScreenBuffer *buffer,
                             unsigned long frame_count),
           void (*ss_cleanup)(void),
           int character_width,
           int delay);

/*
  Write `num_chars` from `chars` to the screen at (x, y), where the origin is
  defined as the upper left corner of the window. The screen location is
  blanked using the padding character before any new characters are written.

  This function is provided for convenience. Copying larger regions of memory
  into the buffer directly will be faster than individual characters if you can
  do it.

  This function does no bounds checking at all, that is up to the caller.
*/
void write_to_buffer(struct ScreenBuffer *sbuffer, char *chars, int num_chars,
                     int x, int y);

#endif  // RENDERER_H_
