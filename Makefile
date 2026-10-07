CFLAGS = -std=c99 -Wall -Wextra -pedantic
CXXFLAGS = -Wall -Wextra -pedantic -std=c++17
LUA_DIR = third_party/lua-5.4.3/src
LUA_CFLAGS = -std=c99 -O2 -DLUA_USE_POSIX
LUA_OBJECTS = $(patsubst %.c,%.o,$(wildcard $(LUA_DIR)/*.c))
CPPFLAGS = -I. -I$(LUA_DIR)
DEPFLAGS = -MMD -MP
LDFLAGS =
LDLIBS = -lm
PLUGIN_EXTENSION = .so
PLUGIN_LDFLAGS = -shared -fPIC

# glibc and musl hide POSIX under -std=c99 unless asked. The BSDs and macOS
# expose everything by default and hide SIGWINCH and TIOCGWINSZ when asked.
# Screensaver libraries call back into strange, so it must export its symbols;
# macOS does that by default.
ifeq ($(shell uname -s),Linux)
CPPFLAGS += -D_XOPEN_SOURCE=700
LDFLAGS += -rdynamic
LDLIBS += -ldl
endif
ifeq ($(shell uname -s),Darwin)
PLUGIN_EXTENSION = .dylib
PLUGIN_LDFLAGS = -dynamiclib -undefined dynamic_lookup
endif
GTEST_PREFIX ?= /opt/homebrew/opt/googletest
GTEST_CPPFLAGS = -I$(GTEST_PREFIX)/include
GTEST_LDLIBS = -L$(GTEST_PREFIX)/lib -lgtest -lgtest_main -pthread

TARGET = strange
OBJECTS = main.o pty/runtime.o pty/pty.o pty/screensaver.o pty/state_machine.o pty/terminal_modes.o pty/watermark.o src/cli.o src/renderer.o src/screensaver_registry.o src/screensaver_loader.o src/lua_screensaver.o src/session.o src/keys.o src/demos/denabase.o src/demos/digital_rain.o $(LUA_OBJECTS)
TEST_TARGET = strange_test
TEST_OBJECTS = tests/cli_test.o tests/runtime_state_test.o tests/renderer_test.o tests/screensaver_registry_test.o tests/screensaver_loader_test.o tests/session_test.o tests/keys_test.o tests/terminal_modes_test.o pty/state_machine.o pty/terminal_modes.o pty/watermark.o src/cli.o src/renderer.o src/screensaver_registry.o src/screensaver_loader.o src/lua_screensaver.o src/session.o src/keys.o src/demos/denabase.o src/demos/digital_rain.o $(LUA_OBJECTS)

EXAMPLES = examples/bounce$(PLUGIN_EXTENSION)
TEST_FIXTURES = tests/fixtures/stale_api$(PLUGIN_EXTENSION)

.PHONY: all clean debug examples test

all: $(TARGET)

examples: $(EXAMPLES)

debug: CFLAGS += -g
debug: LUA_CFLAGS += -g
debug: $(TARGET)

test: $(TEST_TARGET) $(EXAMPLES) $(TEST_FIXTURES)
	./$(TEST_TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(LUA_DIR)/%.o: $(LUA_DIR)/%.c
	$(CC) $(LUA_CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CPPFLAGS) $(DEPFLAGS) $(CFLAGS) -c $< -o $@

examples/%$(PLUGIN_EXTENSION): examples/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PLUGIN_LDFLAGS) $< -o $@

tests/fixtures/%$(PLUGIN_EXTENSION): tests/fixtures/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PLUGIN_LDFLAGS) $< -o $@

tests/%.o: tests/%.cc
	$(CXX) $(CPPFLAGS) $(DEPFLAGS) $(GTEST_CPPFLAGS) $(CXXFLAGS) \
		-DSTRANGE_PLUGIN_EXTENSION='"$(PLUGIN_EXTENSION)"' -c $< -o $@

-include $(OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d)

$(TEST_TARGET): $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(GTEST_LDLIBS) $(LDLIBS)

clean:
	rm -f main.o pty/*.o src/*.o src/demos/*.o tests/*.o $(TARGET) $(TEST_TARGET)
	rm -f main.d pty/*.d src/*.d src/demos/*.d tests/*.d
	rm -f $(LUA_OBJECTS) $(EXAMPLES) $(TEST_FIXTURES)
