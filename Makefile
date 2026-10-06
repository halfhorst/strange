CFLAGS = -std=c99 -Wall -Wextra -pedantic
CXXFLAGS = -Wall -Wextra -pedantic -std=c++17
CPPFLAGS = -I.
LDFLAGS = -lm

# glibc and musl hide POSIX under -std=c99 unless asked. The BSDs and macOS
# expose everything by default and hide SIGWINCH and TIOCGWINSZ when asked.
ifeq ($(shell uname -s),Linux)
CPPFLAGS += -D_XOPEN_SOURCE=700
endif
GTEST_PREFIX ?= /opt/homebrew/opt/googletest
GTEST_CPPFLAGS = -I$(GTEST_PREFIX)/include
GTEST_LDLIBS = -L$(GTEST_PREFIX)/lib -lgtest -lgtest_main -pthread

TARGET = strange
OBJECTS = main.o pty/runtime.o pty/pty.o pty/screensaver.o pty/state_machine.o pty/terminal_modes.o pty/watermark.o src/cli.o src/renderer.o src/screensaver_registry.o src/demos/denabase.o src/demos/digital_rain.o
TEST_TARGET = runtime_state_test
TEST_OBJECTS = tests/cli_test.o tests/runtime_state_test.o tests/renderer_test.o tests/screensaver_registry_test.o tests/terminal_modes_test.o pty/state_machine.o pty/terminal_modes.o pty/watermark.o src/cli.o src/renderer.o src/screensaver_registry.o src/demos/denabase.o src/demos/digital_rain.o

.PHONY: all clean debug test

all: $(TARGET)

debug: CFLAGS += -g
debug: $(TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

tests/%.o: tests/%.cc
	$(CXX) $(CPPFLAGS) $(GTEST_CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(TEST_TARGET): $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(GTEST_LDLIBS) $(LDFLAGS)

clean:
	rm -f main.o pty/*.o src/*.o src/demos/*.o tests/*.o strange $(TEST_TARGET) strangeland foo
