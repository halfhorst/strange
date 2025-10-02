#include "timer.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <util.h>

// Global PTY variables accessible to screensaver.c
int master_fd = -1;
pid_t shell_pid = -1;

void cleanup_pty(int sig) {
  if (master_fd >= 0) {
    close(master_fd);
  }
  if (shell_pid > 0) {
    kill(shell_pid, SIGTERM);
    waitpid(shell_pid, NULL, 0);
  }
  exit(0);
}

int setup_pty_and_shell(void) {
  char slave_name[128];

  // Create PTY
  master_fd = posix_openpt(O_RDWR | O_NOCTTY);
  if (master_fd == -1) {
    perror("posix_openpt");
    return -1;
  }

  if (grantpt(master_fd) == -1 || unlockpt(master_fd) == -1) {
    perror("grantpt/unlockpt");
    close(master_fd);
    return -1;
  }

  if (ptsname_r(master_fd, slave_name, sizeof(slave_name)) != 0) {
    perror("ptsname_r");
    close(master_fd);
    return -1;
  }

  // Fork to create shell process
  shell_pid = fork();
  if (shell_pid == -1) {
    perror("fork");
    close(master_fd);
    return -1;
  }

  if (shell_pid == 0) {
    // Child process - become the shell
    setsid();

    int slave_fd = open(slave_name, O_RDWR);
    if (slave_fd == -1) {
      perror("open slave");
      exit(1);
    }

    dup2(slave_fd, STDIN_FILENO);
    dup2(slave_fd, STDOUT_FILENO);
    dup2(slave_fd, STDERR_FILENO);

    if (slave_fd > 2) {
      close(slave_fd);
    }
    close(master_fd);

    // Execute shell
    char *shell = getenv("SHELL");
    if (!shell)
      shell = "/bin/bash";
    execl(shell, shell, NULL);
    perror("execl");
    exit(1);
  }

  // Make master_fd non-blocking
  int flags = fcntl(master_fd, F_GETFL);
  fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);

  // Set up cleanup handler
  signal(SIGINT, cleanup_pty);
  signal(SIGTERM, cleanup_pty);

  return 0;
}

// void setup_pty(void) {
//     // Legacy function - keep for compatibility
//     int slave_fd;
//     char slave_name[128];

//     // Open a pseudoterminal master
//     master_fd = posix_openpt(O_RDWR | O_NOCTTY);
//     if (master_fd == -1) {
//         perror("posix_openpt");
//         exit(EXIT_FAILURE);
//     }

//     // Grant access to the slave pseudoterminal
//     if (grantpt(master_fd) == -1) {
//         perror("grantpt");
//         close(master_fd);
//         exit(EXIT_FAILURE);
//     }

//     // Unlock the slave pseudoterminal
//     if (unlockpt(master_fd) == -1) {
//         perror("unlockpt");
//         close(master_fd);
//         exit(EXIT_FAILURE);
//     }

//     // Get the name of the slave pseudoterminal
//     if (ptsname_r(master_fd, slave_name, sizeof(slave_name)) != 0) {
//         perror("ptsname_r");
//         close(master_fd);
//         exit(EXIT_FAILURE);
//     }

//     // Open the slave pseudoterminal
//     slave_fd = open(slave_name, O_RDWR | O_NOCTTY);
//     if (slave_fd == -1) {
//         perror("open");
//         close(master_fd);
//         exit(EXIT_FAILURE);
//     }

//     // Set up terminal attributes for the slave pseudoterminal
//     struct termios termios_p;
//     if (tcgetattr(slave_fd, &termios_p) == -1) {
//         perror("tcgetattr");
//         close(master_fd);
//         close(slave_fd);
//         exit(EXIT_FAILURE);
//     }

//     cfmakeraw(&termios_p);
//     if (tcsetattr(slave_fd, TCSANOW, &termios_p) == -1) {
//         perror("tcsetattr");
//         close(master_fd);
//         close(slave_fd);
//         exit(EXIT_FAILURE);
//     }

//     // Example: Write to the slave pseudoterminal
//     const char *message = "Pseudoterminal started.\n";
//     write(slave_fd, message, strlen(message));

//     // Close file descriptors
//     close(master_fd);
//     close(slave_fd);
// }
