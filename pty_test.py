import shutil
import atexit
import termios
import select
import fcntl
import struct
import pty
import tty
import sys
import time
import os

def main():
    pid, fd = pty.fork()

    if pid == 0:
        child()
    
    parent(pid, fd)
    

def child():
    print("Child, signing off.")

    sh = os.environ.get("SHELL")
    if not sh:
        sh = "/bin/sh"
    os.execlp(sh, sh)


def set_winsize(fd, lines, col, xpix=0, ypix=0):
    winsize = struct.pack("HHHH", lines, col, xpix, ypix)
    fcntl.ioctl(fd, termios.TIOCSWINSZ, winsize)


def parent(pid, fd):
    print("\033[2J") # Clear screen
    print("\033[1;0H") # Re-position cursor

    print(f"Parent, checking in. Child fd is {fd}.")

    # set the child to the same size as parent
    tsize = shutil.get_terminal_size()
    set_winsize(fd, tsize.lines, tsize.columns)

    stdin_fd = sys.stdin.fileno()

    # Save existing terminal settings
    # and restore on clean exit
    existing_tty = termios.tcgetattr(stdin_fd)
    atexit.register(lambda: termios.tcsetattr(stdin_fd,
                                              termios.TCSADRAIN,
                                              existing_tty))

    # Set foreground process controlling tty
    # to raw mode. This turns off all the "nice stuff"
    # so you can pipe chars directly to the child,
    # no ANSI escapes or any funny businesse happening
    tty.setraw(stdin_fd)
    
    # Turn on nonblocking IO so I can check parent and child
    # for updates w/o blocking on one or the other
    os.set_blocking(fd, False)
    os.set_blocking(stdin_fd, False)
   
    # Core loop: check for input from stdin, write it to child
    # check for output from child, write it to stdout
    #while True:
    #    try:
    #        user_input = os.read(stdin_fd, 1024)
    #    except BlockingIOError:
    #        user_input = b""
    #    except OSError:  # ctrl+c can put you in a bad spot
    #        exit()
    #    if user_input:
    #        os.write(fd, user_input)

    #    try:
    #        shell_output = os.read(fd, 1024)
    #    except BlockingIOError:
    #        shell_output = b""
    #    except OSError:  # Same as above
    #        exit()
    #    if shell_output:
    #        print(shell_output.decode(), end="", flush=True)

    # let's poll instead.
    epoll = select.epoll(2)
    epoll.register(fd, select.EPOLLIN | select.EPOLLET)
    epoll.register(stdin_fd, select.EPOLLIN | select.EPOLLET)

    while True:
        for fileno, event in epoll.poll():
            if fileno == stdin_fd:
                try:
                    user_input = os.read(stdin_fd, 1024)
                except OSError:
                    exit()
                os.write(fd, user_input)
            elif fileno == fd:
                try:
                    shell_output = os.read(fd, 1024)
                except OSError:
                    exit()
                print(shell_output.decode(), end="", flush=True)

main()

