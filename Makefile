.POSIX:

CC = clang
CCFLAGS = -std=c99 -Wall -Wextra -Werror -pedantic
LDFLAGS = -Iinclude

SRC_DIR=src
BUILD_DIR=build

TARGET = strange
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

all: strange

debug: CFLAGS += -g
debug: strange

strange: $(OBJECTS)
	$(CC) $(CCFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CCFLAGS) -c $< -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) strange

denabase: strange
	./strange  denabase

cube: strange
	./strange --delay=250 cube

digital_rain: strange
	./strange digital_rain

.PHONY: all clean debug denabase cube digital_rain
