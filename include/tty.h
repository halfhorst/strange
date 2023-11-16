#ifndef TTY_H_
#define TTY_H_

#include "renderer.h"

// #define ANSI_POSITION_CURSOR(x, y) \033[x;yH

// char *ANSI_CLEAR_SCREEN;
// char *ANSI_HIDE_CURSOR;
// char *ANSI_SHOW_CURSOR;

/*
  Get the terminal window size in row by column format and return it through
  `w` and `h`. Returns 1 on failure.
*/
int get_window_size(int *w, int *h);

/*
  Get the idle time on the terminal in seconds based on stdin. Returns -1 on
  failure.
*/
int get_idle_seconds();

/*

  Saves current terminal settings and <...>

  Stash the current tty settings in `restore_term` and prepare the terminal for
  rendering in `render_term`. This means turning off line buffering, clearing
  the screen, and hiding the cursor.

  Canonical mode and echo is also turned off for capturing keystrokes though
  this is not currently used.

  Returns 0 on success and 1 on failure.
*/
int prepare_tty();

/*
  Clear the tty of all characters

*/
int clear_tty();

/*
  Print the screen buffer to the tty
*/
int print_to_tty(struct ScreenBuffer *screen_buffer);


/*
  Restore the tty to the settings saved at the time `init_tty` was called. This method is a no-op if `init_tty` was never called. Returns 1 on failure.
*/
int restore_tty();

#endif  // TTY_H_
