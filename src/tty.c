#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "./tty.h"

int get_idle_seconds() {
  char const *tty = ttyname(STDIN_FILENO);
  struct stat sbuf;
  if (stat(tty, &sbuf) == -1) {
    return -1;
  };
  return time(NULL) - sbuf.st_atim.tv_sec;
}

int get_window_size(int *w, int *h) {
  struct winsize ws;
  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) {
    return 1;
  }
  *h = ws.ws_row - 1;
  *w = ws.ws_col - 1;

  return 0;
}

int init_tty(struct termios *render_term, struct termios *restore_term) {
  // Stash current settings
  if (tcgetattr(STDIN_FILENO, render_term) == -1) {
    return 1;
  };
  if (tcgetattr(STDIN_FILENO, restore_term) == -1) {
    return 1;
  };

  // TODO: This is only necessary if capturing keyboard keystrokes.
  // Modify the rendering tty and apply immediately.

  // What we are doing is modifying _input processing_ behavior and applying
  // immediately. We turn off input echoing and "canonical mode." Canonical
  // mode entails grouping input by lines, see below.
  // https://www.gnu.org/software/libc/manual/html_node/Canonical-or-Not.html
  render_term->c_lflag &= ~(ICANON | ECHO);
  if (tcsetattr(STDIN_FILENO, TCSANOW, render_term) == -1) {
    return 1;
  };

  // turn off line buffering
  setbuf(stdout, NULL);
  // clear the screen
  printf("\033[2J");
  // hide the cursor
  printf("\033[?25l");

  return 0;
}

int restore_tty(struct termios *restore_term) {
  // clear the screen
  printf("\033[2J");
  // restore the cursor
  printf("\033[?25h");
  // reset cursor at upper left
  printf("\033[0;0H");
  // restore tty settings re: input processing
  if (tcsetattr(STDIN_FILENO, TCSANOW, restore_term) == -1) {
    return 1;
  };

  return 0;
}
