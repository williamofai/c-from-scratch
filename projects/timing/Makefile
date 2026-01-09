# Makefile for timing - Composed Timing Health Monitor
#
# Module 3 of c-from-scratch
# Composes Pulse (Module 1) and Baseline (Module 2)
#
# Targets:
#   all     - Build everything (default)
#   demo    - Build and run the demo
#   test    - Run all tests
#   clean   - Remove build artifacts

CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c99
CFLAGS += -O2
CFLAGS += -I$(INC_DIR)
CFLAGS += -I../../include

# Stricter flags for development
DEV_FLAGS = -Wshadow -Wconversion -Wdouble-promotion
DEV_FLAGS += -Wformat=2 -Wundef -fno-common

LIBS = -lm

SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build

# Source files - include pulse.c and baseline.c from sibling modules
TIMING_SRCS = $(SRC_DIR)/timing.c
PULSE_SRC = ../pulse/src/pulse.c
BASELINE_SRC = ../baseline/src/baseline.c

DEMO = $(BUILD_DIR)/timing
TEST = $(BUILD_DIR)/test_timing

.PHONY: all demo test clean help

all: $(DEMO) $(TEST)

$(DEMO): $(BUILD_DIR)/timing.o $(BUILD_DIR)/main.o $(BUILD_DIR)/pulse.o $(BUILD_DIR)/baseline.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(TEST): $(BUILD_DIR)/timing.o $(BUILD_DIR)/test_timing.o $(BUILD_DIR)/pulse.o $(BUILD_DIR)/baseline.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(BUILD_DIR)/timing.o: $(SRC_DIR)/timing.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DEV_FLAGS) -I../pulse/include -I../baseline/include -c -o $@ $<

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DEV_FLAGS) -I../pulse/include -I../baseline/include -c -o $@ $<

$(BUILD_DIR)/test_timing.o: $(TEST_DIR)/test_timing.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DEV_FLAGS) -I../pulse/include -I../baseline/include -c -o $@ $<

$(BUILD_DIR)/pulse.o: $(PULSE_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I../pulse/include -c -o $@ $<

$(BUILD_DIR)/baseline.o: $(BASELINE_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I../baseline/include -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

demo: $(DEMO)
	./$(DEMO)

test: $(TEST)
	./$(TEST)

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "Available targets:"
	@echo "  all     - Build demo and test binaries (default)"
	@echo "  demo    - Build and run the demo"
	@echo "  test    - Build and run all tests"
	@echo "  clean   - Remove build artifacts"
	@echo "  help    - Show this message"
