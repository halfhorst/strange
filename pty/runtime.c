#include "runtime.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#include "pty.h"
#include "screensaver.h"

void strange_print_usage(FILE *stream, const char *prog_name) {
  fprintf(stream, "Usage: %s [timeout_seconds]\n", prog_name);
  fprintf(stream,
          "  timeout_seconds: seconds of inactivity before the screensaver "
          "starts (default: %d)\n",
          STRANGE_DEFAULT_TIMEOUT_SECONDS);
  fprintf(stream, "  -h, --help: show this help message\n");
}

int strange_run(const struct strange_options *options) {
  if (setup_pty_and_shell() < 0) {
    fprintf(stderr, "Failed to set up PTY and shell\n");
    return 1;
  }

  int stdin_flags = fcntl(STDIN_FILENO, F_GETFL);
  if (stdin_flags == -1 ||
      fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK) == -1) {
    perror("fcntl");
    cleanup_pty();
    return 1;
  }

  set_screensaver_timeout(options->timeout_seconds);

  int runtime_status = run_screensaver_loop();
  int shell_status = 0;
  int shell_exited = poll_shell_exit(&shell_status);

  if (fcntl(STDIN_FILENO, F_SETFL, stdin_flags) == -1) {
    perror("fcntl");
    runtime_status = 1;
  }

  cleanup_pty();

  if (runtime_status != 0) {
    return 1;
  }
  if (shell_exited == -1) {
    perror("waitpid");
    return 1;
  }
  if (shell_exited == 1) {
    if (WIFEXITED(shell_status)) {
      return WEXITSTATUS(shell_status);
    }
    if (WIFSIGNALED(shell_status)) {
      return 128 + WTERMSIG(shell_status);
    }
  }

  return 0;
}
