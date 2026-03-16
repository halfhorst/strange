CFLAGS = -Wall -Wextra -pedantic
CXXFLAGS = -Wall -Wextra -pedantic -std=c++17
CPPFLAGS = -I.
LDFLAGS = -lm
GTEST_PREFIX ?= /opt/homebrew/opt/googletest
GTEST_CPPFLAGS = -I$(GTEST_PREFIX)/include
GTEST_LDLIBS = -L$(GTEST_PREFIX)/lib -lgtest -lgtest_main -pthread

TARGET = strange
OBJECTS = main.o pty/runtime.o pty/pty.o pty/screensaver.o pty/state_machine.o pty/watermark.o
TEST_TARGET = runtime_state_test
TEST_OBJECTS = tests/runtime_state_test.o pty/state_machine.o

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
	$(CXX) $(CXXFLAGS) $^ -o $@ $(GTEST_LDLIBS)

clean:
	rm -f main.o pty/*.o src/*.o src/demos/*.o tests/*.o strange $(TEST_TARGET) strangeland foo
