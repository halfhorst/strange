#include "pty.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

int master_fd = -1;
pid_t shell_pid = -1;

static volatile sig_atomic_t shutdown_requested = 0;
static int shell_status_ready = 0;
static int last_shell_status = 0;

static void request_shutdown(int sig) {
  (void)sig;
  shutdown_requested = 1;
}

static int install_signal_handler(int signo) {
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = request_shutdown;

  if (sigemptyset(&action.sa_mask) == -1) {
    return -1;
  }

  return sigaction(signo, &action, NULL);
}

static int install_signal_handlers(void) {
  if (install_signal_handler(SIGINT) == -1) {
    return -1;
  }
  if (install_signal_handler(SIGTERM) == -1) {
    return -1;
  }
  if (install_signal_handler(SIGHUP) == -1) {
    return -1;
  }

  return 0;
}

static int shell_path_is_valid(const char *shell_path) {
  struct stat st;

  if (shell_path == NULL || shell_path[0] != '/') {
    return 0;
  }

  if (stat(shell_path, &st) == -1) {
    return 0;
  }

  if (!S_ISREG(st.st_mode)) {
    return 0;
  }

  return access(shell_path, X_OK) == 0;
}

static const char *resolve_shell_path(void) {
  const char *shell_path = getenv("SHELL");

  if (shell_path_is_valid(shell_path)) {
    return shell_path;
  }

  return "/bin/sh";
}

static const char *shell_basename(const char *shell_path) {
  const char *slash = strrchr(shell_path, '/');

  if (slash == NULL || slash[1] == '\0') {
    return shell_path;
  }

  return slash + 1;
}

static int build_login_argv0(const char *shell_path, char *argv0,
                             size_t argv0_size) {
  int written = snprintf(argv0, argv0_size, "-%s", shell_basename(shell_path));

  if (written < 0 || (size_t)written >= argv0_size) {
    errno = ENAMETOOLONG;
    return -1;
  }

  return 0;
}

static int copy_terminal_state(int slave_fd) {
  struct termios terminal_state;

  if (tcgetattr(STDIN_FILENO, &terminal_state) == -1) {
    return -1;
  }

  if (tcsetattr(slave_fd, TCSANOW, &terminal_state) == -1) {
    return -1;
  }

  struct winsize terminal_size;
  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &terminal_size) == 0 &&
      ioctl(slave_fd, TIOCSWINSZ, &terminal_size) == -1) {
    return -1;
  }

  return 0;
}

static int set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL);

  if (flags == -1) {
    return -1;
  }

  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void close_master_fd(void) {
  if (master_fd >= 0) {
    close(master_fd);
    master_fd = -1;
  }
}

static void record_shell_exit_status(int status) {
  last_shell_status = status;
  shell_status_ready = 1;
  shell_pid = -1;
}

int strange_shutdown_requested(void) {
  return shutdown_requested != 0;
}

int poll_shell_exit(int *status) {
  int wait_status = 0;

  if (shell_status_ready) {
    if (status != NULL) {
      *status = last_shell_status;
    }
    return 1;
  }

  if (shell_pid <= 0) {
    return 0;
  }

  pid_t result = waitpid(shell_pid, &wait_status, WNOHANG);

  if (result == 0) {
    return 0;
  }
  if (result == -1) {
    if (errno == ECHILD) {
      shell_pid = -1;
      return 0;
    }
    return -1;
  }

  record_shell_exit_status(wait_status);

  if (status != NULL) {
    *status = wait_status;
  }

  return 1;
}

static void exec_login_shell_or_die(const char *shell_path,
                                    const char *slave_name) {
  if (setsid() == -1) {
    perror("setsid");
    _exit(EXIT_FAILURE);
  }

  int slave_fd = open(slave_name, O_RDWR);
  if (slave_fd == -1) {
    perror("open");
    _exit(EXIT_FAILURE);
  }

  if (ioctl(slave_fd, TIOCSCTTY, 0) == -1) {
    perror("TIOCSCTTY");
    _exit(EXIT_FAILURE);
  }

  if (copy_terminal_state(slave_fd) == -1) {
    perror("terminal setup");
    _exit(EXIT_FAILURE);
  }

  if (dup2(slave_fd, STDIN_FILENO) == -1 || dup2(slave_fd, STDOUT_FILENO) == -1 ||
      dup2(slave_fd, STDERR_FILENO) == -1) {
    perror("dup2");
    _exit(EXIT_FAILURE);
  }

  if (slave_fd > STDERR_FILENO) {
    close(slave_fd);
  }

  close(master_fd);

  char login_argv0[PATH_MAX];
  if (build_login_argv0(shell_path, login_argv0, sizeof(login_argv0)) == -1) {
    perror("shell argv");
    _exit(EXIT_FAILURE);
  }

  char *const argv[] = {login_argv0, NULL};
  execv(shell_path, argv);

  perror("execv");
  _exit(EXIT_FAILURE);
}

int setup_pty_and_shell(void) {
  char slave_name[128];
  const char *shell_path = resolve_shell_path();

  shutdown_requested = 0;
  shell_status_ready = 0;
  last_shell_status = 0;

  if (!shell_path_is_valid(shell_path)) {
    errno = ENOENT;
    return -1;
  }

  master_fd = posix_openpt(O_RDWR | O_NOCTTY);
  if (master_fd == -1) {
    perror("posix_openpt");
    return -1;
  }

  if (grantpt(master_fd) == -1 || unlockpt(master_fd) == -1) {
    perror("grantpt/unlockpt");
    close_master_fd();
    return -1;
  }

  if (ptsname_r(master_fd, slave_name, sizeof(slave_name)) != 0) {
    perror("ptsname_r");
    close_master_fd();
    return -1;
  }

  shell_pid = fork();
  if (shell_pid == -1) {
    perror("fork");
    close_master_fd();
    return -1;
  }

  if (shell_pid == 0) {
    exec_login_shell_or_die(shell_path, slave_name);
  }

  if (set_nonblocking(master_fd) == -1) {
    perror("fcntl");
    cleanup_pty();
    return -1;
  }

  if (install_signal_handlers() == -1) {
    perror("sigaction");
    cleanup_pty();
    return -1;
  }

  return 0;
}

void cleanup_pty(void) {
  pid_t pid = shell_pid;
  int wait_status = 0;
  int reaped = 0;
  pid_t wait_result = -1;

  (void)poll_shell_exit(NULL);
  pid = shell_pid;

  close_master_fd();

  if (pid > 0 && !shell_status_ready) {
    if (kill(pid, SIGHUP) == -1 && errno != ESRCH) {
      perror("kill");
    }
    if (kill(pid, SIGCONT) == -1 && errno != ESRCH) {
      perror("kill");
    }

    while ((wait_result = waitpid(pid, &wait_status, 0)) == -1) {
      if (errno == EINTR) {
        continue;
      }
      if (errno != ECHILD) {
        perror("waitpid");
      }
      break;
    }

    if (wait_result == pid) {
      reaped = 1;
    }
  }

  if (reaped) {
    last_shell_status = wait_status;
    shell_status_ready = 1;
  }

  shell_pid = -1;
}
