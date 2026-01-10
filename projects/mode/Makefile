# Module 7: Mode Manager
# The "Captain" of the safety-critical ship

CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c11 -O2
INCLUDES = -Iinclude

SRC_DIR = src
BUILD_DIR = build

.PHONY: all clean demo test

all: demo test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Demo executable
demo: $(BUILD_DIR)/mode_demo
	@echo ""
	@echo "Running Mode Manager Demo..."
	@echo "============================"
	@./$(BUILD_DIR)/mode_demo

$(BUILD_DIR)/mode_demo: $(SRC_DIR)/mode.c $(SRC_DIR)/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

# Test executable
test: $(BUILD_DIR)/test_mode
	@echo ""
	@echo "Running Contract Tests..."
	@echo "========================="
	@./$(BUILD_DIR)/test_mode

$(BUILD_DIR)/test_mode: $(SRC_DIR)/mode.c tests/test_mode.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

clean:
	rm -rf $(BUILD_DIR)
