.POSIX:

CC = gcc
CFLAGS = -Iinclude -Iinclude/demos -Wall -pedantic
LDFLAGS =
LDLIBS = -lm

OBJECTS = cli.o tty.o metrics.o renderer.o slsignal.o timing.o denabase.o digital_rain.o

all: strange

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
metrics.o: src/metrics.c include/metrics.h
	$(CC) $(CFLAGS) -c $<
denabase.o: src/demos/denabase.c include/demos/denabase.h
	$(CC) $(CFLAGS) -c $<
digital_rain.o: src/demos/digital_rain.c include/demos/digital_rain.h
	$(CC) $(CFLAGS) -c $<

tty_source = src/tty_unix.c
slsignal_source = src/slsignal_unix.c
timing_source = src/timing_unix.c
ifeq ($(OS),Windows_NT)
	tty_source = src/tty_win.c
	slsignal_source = src/slsignal_win.c
endif

tty.o: $(tty_source) include/tty.h
	$(CC) $(CFLAGS) -o $@ -c $<
slsignal.o: $(slsignal_source) include/slsignal.h
	$(CC) $(CFLAGS) -o $@ -c $<
timing.o: $(timing_source) include/timing.h
	$(CC) $(CFLAGS) -o $@ -c $<



# renderer.o: $(renderer_source) include/renderer.h
# 	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f *.o strangeland
