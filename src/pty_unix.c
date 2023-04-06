#define _XOPEN_SOURCE 600

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

    //    An unused UNIX 98 pseudoterminal master is opened by calling
    //    posix_openpt(3).  (This function opens the master clone device,
    //    /dev/ptmx; see pts(4).)  After performing any program-specific
    //    initializations, changing the ownership and permissions of the
    //    slave device using grantpt(3), and unlocking the slave using
    //    unlockpt(3)), the corresponding slave device can be opened by
    //    passing the name returned by ptsname(3) in a call to open(2).

int PTY_NAME_BUFF_LEN = 40;

int init_pty_leader();
int gather_follower(int leaderFd, char *followerName);
pid_t fork_pty(int leaderFd, char* followerName, const struct termios *followerTermios, const struct winsize *followerWs);

int main(void) {
    
    char followerName[PTY_NAME_BUFF_LEN];

    // save current termios settings

    int leaderFd = init_pty_leader();
    int result = gather_follower(leaderFd, followerName);
    pid_t childPid = fork_pty(leaderFd, followerName, NULL, NULL);

    printf("%s\n", followerName);
    // printf("%li\n", strlen(followerName));

    return 0;
}


int init_pty_leader() {
    int result;

    int fd = posix_openpt(O_RDWR);

    if (fd == -1) {
        return -1;
    }

    result = grantpt(fd);

    if (result == -1) {
        close(fd);
        return -1;
    }

    result = unlockpt(fd);

    if (result == -1) {
        close(fd);
        return -1;
    }

    return fd;
}

int gather_follower(int leaderFd, char *followerName) {
    
    char *rep_name = ptsname(leaderFd);

    if (rep_name == NULL) {
        return -1;
    }

    if(strlen(rep_name) > PTY_NAME_BUFF_LEN ) {
        errno = EOVERFLOW;
        return -1;
    }
    strncpy(followerName, rep_name, strlen(rep_name));

    return 0;
}

pid_t fork_pty(int leaderFd, char* followerName, const struct termios *followerTermios, const struct winsize *followerWs) {

    int savedError;

    pid_t pid = fork();

    if (pid == -1) {
        savedError = errno;
        close(leaderFd);
        errno = savedError;
        return -1;
    }

    // parent
    if (pid != 0) {
        return pid;
    }

    // child

    // start new session -- what it do
    if (setsid() == -1) {
        exit(-1);  // TODO: err_exit from book
    }

    close(leaderFd);

    int followerFd = open(followerName, O_RDWR);
    if (followerFd == -1) {
        exit(-1);  // TODO: err_exit
    }

    if (followerTermios != NULL) {
        if (tcsetattr(followerFd, TCSANOW, followerTermios) == -1) {
            exit(-1);  // TODO err_exit
        }
    }

    if (followerWs != NULL) {
        if (ioctl(followerFd, TIOCSWINSZ, followerWs) == -1) {
            exit(-1);  // TODO err_exit
        }
    }

}