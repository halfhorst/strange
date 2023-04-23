#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "tty.h"
#include "renderer.h"

static char *ANSI_CLEAR_SCREEN = "\033[2J";
static char *ANSI_HIDE_CURSOR = "\033[?25l";
static char *ANSI_SHOW_CURSOR = "\033[?25h";
static char *ANSI_POSITION_TOP_LEFT = "\033[1;0H";

static struct termios render_term, restore_term;


int get_idle_seconds() {
  // char const *tty = ttyname(STDIN_FILENO);
  // struct stat sbuf;
  // if (stat(tty, &sbuf) == -1) {
  //   return -1;
  // };
  // return time(NULL) - sbuf.st_atim.tv_sec;
  return 0;
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

int prepare_tty() {
  // Stash current settings
  if (tcgetattr(STDIN_FILENO, &render_term) == -1) {
    return 1;
  };
  if (tcgetattr(STDIN_FILENO, &restore_term) == -1) {
    return 1;
  };

  // TODO: This is only necessary if capturing keyboard keystrokes.
  // Modify the rendering tty and apply immediately.

  // What we are doing is modifying _input processing_ behavior and applying
  // immediately. We turn off input echoing and "canonical mode." Canonical
  // mode entails grouping input by lines, see below.
  // https://www.gnu.org/software/libc/manual/html_node/Canonical-or-Not.html
  render_term.c_lflag &= ~(ICANON | ECHO);
  if (tcsetattr(STDIN_FILENO, TCSANOW, &render_term) == -1) {
    return 1;
  };

  setbuf(stdout, NULL); // turn off line buffering
  printf("%s", ANSI_CLEAR_SCREEN);
  printf("%s", ANSI_HIDE_CURSOR);

  return 0;
}

int clear_tty() {
  printf("%s", ANSI_CLEAR_SCREEN);
  return 0;
}


int print_to_tty(struct ScreenBuffer *screen_buffer) {
  printf("%s", ANSI_POSITION_TOP_LEFT);

  for (int i = 0; i < screen_buffer->h; i++) {
    // fwrite will write past \0, which is great because it's my
    // foolproof padding char
    fwrite(screen_buffer->buffer + (i * screen_buffer->w * screen_buffer->character_width),
           1, (screen_buffer->w * screen_buffer->character_width), stdout);
    printf("\n");
  }

  return 0;
}

int restore_tty(struct termios *restore_term) {
  printf("%s", ANSI_CLEAR_SCREEN);
  printf("%s", ANSI_SHOW_CURSOR);
  printf("%s", ANSI_POSITION_TOP_LEFT);
  // restore input processing tty settings
  if (tcsetattr(STDIN_FILENO, TCSANOW, restore_term) == -1) {
    return 1;
  };

  return 0;
}
