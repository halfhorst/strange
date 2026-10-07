#ifndef STRANGE_PTY_H_
#define STRANGE_PTY_H_

#include <sys/types.h>

extern int strange_pty_master_fd;
extern pid_t strange_pty_shell_pid;

int strange_pty_install_signal_handlers(void);
int strange_pty_start_shell(void);
void strange_pty_cleanup(void);
int strange_pty_poll_shell_exit(int *status);
int strange_pty_shutdown_requested(void);
int strange_pty_consume_resize_event(void);
int strange_pty_sync_window_size(int misreport_height);

#endif
