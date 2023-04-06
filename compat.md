
**Can ditch standard library + extensions and implement just what I need directly?**
    https://nullprogram.com/blog/2023/02/11/

MacOS has mostly BSD Interface, some Mach. [Here](https://developer.apple.com/documentation/kernel)
Linux - target the linux kernel interface. probably posix too?
Windows - Win32

`apropos . | grep '(2)' | less` for all system calls (I think). Section 4 is the kernel interface. Wouldn't that be system calls?

---

Mac C standard library : LibSystem.dylib

Gnulib -- compatability layer for ISO C and Posix
    https://www.gnu.org/software/gnulib/manual/html_node/Finding-POSIX-substitutes.html#Finding-POSIX-substitutes
    https://gcc.gnu.org/onlinedocs/gcc/Standards.html

SUS - Single Unix Specification. This is somehow related to POSIX. beginning version _3_.
    https://en.wikipedia.org/wiki/Single_UNIX_Specification

**Headers**
argp - glibc extension?
    getopt is alternative posix api
math - ISO C
signal - ISO C
stdbool - ISO C
stdio - ISO C
stdlib - ISO C
string - ISO C
sys/ioctl - "Version 7 AT&T Unix" ??? Unix spec v2, Pre-ISO C?
    apparently supported by most unix-like systems and MacOS. Win32 is different.
sys/stat - NetBSD/FreeBSD ??? Also Unix spec v2?
termio - is this a typo?
termios - Unix spec v2? XBD?
termio vs termios - 
time - ISO C
    https://en.wikipedia.org/wiki/C_date_and_time_functions
unistd - POSIX (OS API). Seems not portable, but just constants?

## Termio and Termios

>  As with signals, terminal I/O control has three different implementations under SVR4, BSD, and POSIX.1. 

SVR4 - `termio`
BSD - `sgtty`
POSIX - `termios`

> Under Linux, both POSIX.1 termios and SVR4 termio are supported directly by the kernel. 

## Signal

> The behavior of signal() varies across UNIX versions, and has also varied historically across different versions of Linux.  Avoid its use: use sigaction(2) instead.  See Portability below.

See the "portability" section of `man 2 signal`.

## ANSI Escape Codes

I assume these are available for now. Not sure about the windows story.

## MacOs

`otool -L`
    for linked libs

`sudo ktrace trace -n -p a.out -c ./a.out > trace.out`
    trace system calls 

## Windows

pty: https://devblogs.microsoft.com/commandline/windows-command-line-introducing-the-windows-pseudo-console-conpty/

## Building

`make` on posix, `nmake` on windows? Or Meson for both.

https://makefiletutorial.com/