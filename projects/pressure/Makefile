# Pressure - Bounded Queue with Backpressure
# Module 6 of c-from-scratch
#
# Usage:
#   make          - Build demo and tests
#   make demo     - Build and run demo
#   make test     - Build and run tests
#   make clean    - Remove build artifacts

CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c11 -O2
CFLAGS_DEBUG = -Wall -Wextra -pedantic -std=c11 -g -O0

# Directories
SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build

# Include path
INCLUDES = -I$(INC_DIR)

# Targets
DEMO = $(BUILD_DIR)/pressure
TEST = $(BUILD_DIR)/test_contracts

.PHONY: all clean demo test

all: $(DEMO) $(TEST)

$(DEMO): $(BUILD_DIR)/pressure.o $(BUILD_DIR)/main.o
	$(CC) $(CFLAGS) -o $@ $^

$(TEST): $(BUILD_DIR)/pressure.o $(BUILD_DIR)/test_pressure.o
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/pressure.o: $(SRC_DIR)/pressure.c $(INC_DIR)/pressure.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c $(INC_DIR)/pressure.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD_DIR)/test_pressure.o: $(TEST_DIR)/test_pressure.c $(INC_DIR)/pressure.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

demo: $(DEMO)
	@echo ""
	@echo "Running Pressure Demo..."
	@echo "========================"
	@./$(DEMO)

test: $(TEST)
	@echo ""
	@echo "Running Contract Tests..."
	@echo "========================="
	@./$(TEST)

clean:
	rm -rf $(BUILD_DIR)

debug: CFLAGS = $(CFLAGS_DEBUG)
debug: clean all
