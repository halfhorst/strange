#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "pty.h"
#include "watermark.h"

struct winsize w;

// > Why use STDOUT_FILENO instead of master_fd? what's the difference?

// ⏺ In this PTY setup:

// - STDOUT_FILENO (file descriptor 1) is the actual terminal where the user
// sees output
// - master_fd is the PTY master side that's connected to the shell process
// running inside the PTY

// When you write to master_fd, you're sending data into the shell (like typing
// input). When you write to STDOUT_FILENO, you're writing directly to the
// user's terminal.

// Looking at screensaver.c lines 178-180, when the shell outputs something, it
// reads from master_fd and writes to STDOUT_FILENO to display it. The
// screensaver itself (lines 59-79) writes directly to STDOUT_FILENO using
// printf.

// For your watermark to appear on screen during the screensaver, it needs to
// write to STDOUT_FILENO (the actual terminal), not master_fd (which would send
// it to the shell process, and the screensaver isn't forwarding that output
// anyway - see line 178-181, output from master_fd is only forwarded when
// !screensaver_active).

// Same reason for the ioctl - you want the window size of the actual terminal
// (STDOUT_FILENO), not the PTY.

// In theory fprintf + fflush should work since fflush forces the buffered data
// out. In fact, the rest of the screensaver code uses printf + fflush (lines
// 59-79 in screensaver.c).

// So yes, fprintf(stdout, ...) + fflush(stdout) would probably be sufficient
// here. The write() approach is just slightly more direct and avoids the stdio
// layer entirely, but either should work in this case.
void display_exit_instructions(void) {

  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  uint16_t width = 17;
  uint16_t right_shift = w.ws_col - width;

  char spaces[width];
  memset(spaces, ' ', width - 1);
  spaces[width] = '\0';

  fprintf(stdout, "\033[1;%dH%s", right_shift, spaces);
  fprintf(stdout, "\033[2;%dH%s", right_shift, " Ctrl+? to exit ");
  fprintf(stdout, "\033[3;%dH%s", right_shift, spaces);
  fflush(stdout);
}
