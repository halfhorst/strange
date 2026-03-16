#include "screensaver.h"

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "watermark.h"

static struct termios orig_termios;
static int raw_mode_enabled = 0;
static int screensaver_visible = 0;
static time_t screensaver_started_at = 0;

void disable_raw_mode(void) {
  if (!raw_mode_enabled) {
    return;
  }

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
  raw_mode_enabled = 0;
}

void enable_raw_mode(void) {
  if (raw_mode_enabled) {
    return;
  }

  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disable_raw_mode);

  struct termios raw = orig_termios;
  cfmakeraw(&raw);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  raw_mode_enabled = 1;
}

void enter_screensaver(void) {
  if (screensaver_visible) {
    return;
  }

  screensaver_visible = 1;
  screensaver_started_at = time(NULL);

  printf("\033[?1049h");
  printf("\033[2J\033[H");
  printf("\033[?25l");
  fflush(stdout);
}

void leave_screensaver(void) {
  if (!screensaver_visible) {
    return;
  }

  screensaver_visible = 0;

  printf("\033[?25h");
  printf("\033[?1049l");
  fflush(stdout);
}

void render_screensaver_frame(const struct timespec *now) {
  if (!screensaver_visible) {
    return;
  }

  time_t now_seconds = now != NULL ? now->tv_sec : time(NULL);
  int elapsed = (int)(now_seconds - screensaver_started_at);
  static const char spinner[] = "|/-\\";
  int spinner_index = elapsed % 4;

  printf("\033[H");
  printf("Strange... %c\n", spinner[spinner_index]);
  printf("Elapsed: %d seconds\n", elapsed);
  display_exit_instructions();
  fflush(stdout);
}
