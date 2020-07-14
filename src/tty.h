#include <termio.h>
#include <unistd.h>

#ifndef TTY_H_
#define TTY_H_

/*
  Get the terminal window size in row by column format and return it through
  `w` and `h`. The return value is 0 on success and 1 on failure.
*/
int get_window_size(int *w, int *h);

/*
  Get the idle time on the terminal in seconds based on stdin. Returns -1 on
  failure.
*/
int get_idle_seconds();

/*
  Stash the current tty settings in `restore_term` and prepare the terminal for
  rendering in `render_term`. This means turning off line buffering, clearing
  the screen, and hiding the cursor.

  Canonical mode and echo is also turned off for capturing keystrokes though
  this is not currently used.

  Returns 0 on success and 1 on failure.
*/
int init_tty(struct termios *render_term, struct termios *restore_term);

/*
  Restore the tty to the settings stored in `restore_term. Returns 0 on
  success, 1 otherwise.
*/
int restore_tty(struct termios *restore_term);

#endif  // TTY_H_
