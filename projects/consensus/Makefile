# Consensus - Triple Modular Redundancy Voter
# Module 5 of c-from-scratch
#
# Usage:
#   make          - Build demo and tests
#   make demo     - Build and run demo
#   make test     - Build and run tests
#   make clean    - Remove build artifacts

CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c99 -O2
CFLAGS_DEBUG = -Wall -Wextra -pedantic -std=c99 -g -O0

# Directories
SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build

# Include path
INCLUDES = -I$(INC_DIR)

# Source files
SRCS = $(SRC_DIR)/consensus.c
MAIN_SRC = $(SRC_DIR)/main.c
TEST_SRC = $(TEST_DIR)/test_consensus.c

# Object files
OBJS = $(BUILD_DIR)/consensus.o $(BUILD_DIR)/main.o
TEST_OBJS = $(BUILD_DIR)/consensus.o $(BUILD_DIR)/test_consensus.o

# Targets
DEMO = $(BUILD_DIR)/consensus
TEST = $(BUILD_DIR)/test_contracts

# Libraries
LIBS = -lm

.PHONY: all clean demo test check

all: $(DEMO) $(TEST)

# Build demo executable
$(DEMO): $(BUILD_DIR)/consensus.o $(BUILD_DIR)/main.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Build test executable
$(TEST): $(BUILD_DIR)/consensus.o $(BUILD_DIR)/test_consensus.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Compile consensus.c
$(BUILD_DIR)/consensus.o: $(SRC_DIR)/consensus.c $(INC_DIR)/consensus.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# Compile main.c
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c $(INC_DIR)/consensus.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# Compile test_consensus.c
$(BUILD_DIR)/test_consensus.o: $(TEST_DIR)/test_consensus.c $(INC_DIR)/consensus.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Run demo
demo: $(DEMO)
	@echo ""
	@echo "Running Consensus Demo..."
	@echo "========================="
	@./$(DEMO)

# Run tests
test: $(TEST)
	@echo ""
	@echo "Running Contract Tests..."
	@echo "========================="
	@./$(TEST)

# Static analysis (if cppcheck available)
check:
	@echo "Running static analysis..."
	@command -v cppcheck >/dev/null 2>&1 && \
		cppcheck --enable=all --suppress=missingIncludeSystem \
		$(INCLUDES) $(SRC_DIR)/*.c $(TEST_DIR)/*.c || \
		echo "cppcheck not installed, skipping"

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Debug build
debug: CFLAGS = $(CFLAGS_DEBUG)
debug: clean all

# Shared library for Python simulator
lib: $(BUILD_DIR)/consensus.o
	$(CC) -shared -fPIC -o $(BUILD_DIR)/libconsensus.so $^ $(LIBS)
