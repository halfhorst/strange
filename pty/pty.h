
#include <sys/types.h>

// TODO: Make a getter
extern int master_fd;
extern pid_t shell_pid;

// PTY management functions
// void setup_pty(void);
int setup_pty_and_shell(void);
void cleanup_pty(int sig);
