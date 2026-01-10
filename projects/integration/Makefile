# c-from-scratch Integration Example
# Complete Safety Monitoring System using all 6 modules
#
# Usage:
#   make          - Build the safety monitor
#   make run      - Build and run
#   make clean    - Remove build artifacts

CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c99 -O2
LDFLAGS = -lm

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

INCLUDES = -I$(INC_DIR)

SOURCES = $(SRC_DIR)/safety_monitor.c \
          $(SRC_DIR)/drift.c \
          $(SRC_DIR)/consensus.c \
          $(SRC_DIR)/pressure.c

TARGET = $(BUILD_DIR)/safety_monitor

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(SOURCES) $(LDFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: $(TARGET)
	@./$(TARGET)

clean:
	rm -rf $(BUILD_DIR)
