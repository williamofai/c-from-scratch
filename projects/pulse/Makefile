# Makefile for pulse - Heartbeat Liveness Monitor
#
# Targets:
#   all     - Build everything (default)
#   test    - Run all tests
#   clean   - Remove build artifacts
#   check   - Static analysis
#   format  - Format source code

CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c99
CFLAGS += -O2
CFLAGS += -D_POSIX_C_SOURCE=199309L
CFLAGS += -I$(INC_DIR)

# Stricter flags for development
DEV_FLAGS = -Wshadow -Wconversion -Wdouble-promotion
DEV_FLAGS += -Wformat=2 -Wundef -fno-common

SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build

SRCS = $(SRC_DIR)/pulse.c $(SRC_DIR)/main.c
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TARGET = $(BUILD_DIR)/pulse

.PHONY: all test clean check format help

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DEV_FLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

test: $(TARGET)
	@echo "Running contract tests..."
	@if [ -f $(TEST_DIR)/test_contracts.c ]; then \
		$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_contracts \
			$(TEST_DIR)/test_contracts.c $(SRC_DIR)/pulse.c && \
		$(BUILD_DIR)/test_contracts; \
	else \
		echo "Tests not yet implemented - see Lesson 5"; \
	fi

clean:
	rm -rf $(BUILD_DIR)

check:
	@echo "Running static analysis..."
	@command -v cppcheck >/dev/null 2>&1 && \
		cppcheck --enable=all --std=c99 --quiet -I$(INC_DIR) $(SRC_DIR)/*.c || \
		echo "cppcheck not installed, skipping"

format:
	@command -v clang-format >/dev/null 2>&1 && \
		clang-format -i $(SRC_DIR)/*.c $(INC_DIR)/*.h || \
		echo "clang-format not installed, skipping"

help:
	@echo "Available targets:"
	@echo "  all     - Build the pulse binary (default)"
	@echo "  test    - Run all tests"
	@echo "  clean   - Remove build artifacts"
	@echo "  check   - Run static analysis (requires cppcheck)"
	@echo "  format  - Format source code (requires clang-format)"
	@echo "  help    - Show this message"

# Dependencies
$(BUILD_DIR)/pulse.o: $(INC_DIR)/pulse.h
$(BUILD_DIR)/main.o: $(INC_DIR)/pulse.h
