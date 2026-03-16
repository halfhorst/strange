#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "pty.h"
#include "watermark.h"

struct winsize w;

void display_exit_instructions(void) {
  enum { kInstructionWidth = 18 };
  const uint16_t width = kInstructionWidth;
  uint16_t left = 1;

  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  if (w.ws_col > width) {
    left = (uint16_t)(w.ws_col - width + 1);
  }

  char spaces[kInstructionWidth + 1];
  memset(spaces, ' ', width - 1);
  spaces[width] = '\0';

  fprintf(stdout, "\033[1;%uH%s", left, spaces);
  fprintf(stdout, "\033[2;%uH%s", left, " any key wakes ");
  fprintf(stdout, "\033[3;%uH%s", left, " Ctrl-Q disables ");
  fflush(stdout);
}
