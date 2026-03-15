CCFLAGS = -Wall -Wextra -pedantic
LDFLAGS = -lm

TARGET = strange
OBJECTS = main.o pty/runtime.o pty/pty.o pty/screensaver.o pty/watermark.o

.PHONY: all clean debug

all: $(TARGET)

debug: CCFLAGS += -g
debug: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CCFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CCFLAGS) -c $< -o $@

clean:
	rm -f main.o pty/*.o src/*.o src/demos/*.o strange strangeland foo
