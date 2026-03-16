#ifndef STRANGE_PTY_H_
#define STRANGE_PTY_H_

#include <sys/types.h>

extern int master_fd;
extern pid_t shell_pid;

int setup_pty_and_shell(void);
void cleanup_pty(void);
int poll_shell_exit(int *status);
int strange_shutdown_requested(void);
int strange_consume_resize_event(void);
int strange_sync_pty_window_size(void);

#endif
