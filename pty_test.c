#include "pty.h"
#include "stdio.h"

int child(void);
int parent(pid_t child_pid);

// Compile with -lutil....
int main(int argc, char **argv)
{

    int master_fd;

    pid_t pid = forkpty(&master_fd, NULL, NULL, NULL);

    if (pid == 0)
    {
        child();
    }

    parent(pid);
}

int child(void)
{

    return 1;
}

int parent(pid_t child_pid)
{

    return 1;
}
