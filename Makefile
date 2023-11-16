.POSIX:

CC = gcc
CFLAGS = -Iinclude -Iinclude/demos -Wall -pedantic
LDFLAGS =
LDLIBS = -lm

OBJECTS = cli.o tty.o renderer.o cleanup.o timing.o screensaver.o denabase.o digital_rain.o

all: strange

.PHONY: debug benchmark

debug: CFLAGS += -g
debug: strange

benchmark: CFLAGS += -DDEBUG -pg
benchmark: strange

# -fsanitize=address
# -fsanitize=undefined
# -fsanitize=leak

strange: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

cli.o: src/cli.c
	$(CC) $(CFLAGS) -c $<
renderer.o: src/renderer.c include/renderer.h
	$(CC) $(CFLAGS) -c $<
screensaver.o: src/screensaver.c include/screensaver.h
	$(CC) $(CFLAGS) -c $<
denabase.o: src/demos/denabase.c include/demos/denabase.h
	$(CC) $(CFLAGS) -c $<
digital_rain.o: src/demos/digital_rain.c include/demos/digital_rain.h
	$(CC) $(CFLAGS) -c $<

tty_source = src/tty_unix.c
cleanup_source = src/cleanup_unix.c
timing_source = src/timing_unix.c
# ifneq ($(OS),Windows_NT)
# strange:
# 	echo "Windows is not supported at this timing, sorry!"
# endif

tty.o: $(tty_source) include/tty.h
	$(CC) $(CFLAGS) -o $@ -c $<
cleanup.o: $(cleanup_source) include/cleanup.h
	$(CC) $(CFLAGS) -o $@ -c $<
timing.o: $(timing_source) include/timing.h
	$(CC) $(CFLAGS) -o $@ -c $<



# renderer.o: $(renderer_source) include/renderer.h
# 	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f *.o strangeland
