.POSIX:

CC = clang
CCFLAGS = -std=c99 -Wall -Wextra -pedantic # -fsanitize=address,undefined,leak
LDFLAGS = -Iinclude

SRC_DIR=src
BUILD_DIR=build

TARGET = strange
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

DEMO_SOURCES = $(wildcard $(SRC_DIR)/demos/*.c)
DEMO_OBJECTS = $(DEMO_SOURCES:$(SRC_DIR)/demo/%.c=$(BUILD_DIR)/%.o)

all: strange

debug: CFLAGS += -g
debug: strange

clangd:
	bear --output build/compile_commands.json -- make

analyze:
	scan-build make

strange: $(OBJECTS) $(DEMO_OBJECTS)
	$(CC) $(CCFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CCFLAGS) -c $< -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) strange

denabase: strange
	./strange -s denabase -d 10

cube: strange
	./strange -s cube -d 10

digital_rain: strange
	./strange -s digital_rain -d 10

.PHONY: all clangd strange clean debug denabase cube digital_rain
