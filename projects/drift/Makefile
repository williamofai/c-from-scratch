# Drift - Rate & Trend Detection Monitor
# Module 4 of c-from-scratch
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
SRCS = $(SRC_DIR)/drift.c
MAIN_SRC = $(SRC_DIR)/main.c
TEST_SRC = $(TEST_DIR)/test_drift.c

# Object files
OBJS = $(BUILD_DIR)/drift.o $(BUILD_DIR)/main.o
TEST_OBJS = $(BUILD_DIR)/drift.o $(BUILD_DIR)/test_drift.o

# Targets
DEMO = $(BUILD_DIR)/drift
TEST = $(BUILD_DIR)/test_contracts

# Libraries
LIBS = -lm

.PHONY: all clean demo test check

all: $(DEMO) $(TEST)

# Build demo executable
$(DEMO): $(BUILD_DIR)/drift.o $(BUILD_DIR)/main.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Build test executable
$(TEST): $(BUILD_DIR)/drift.o $(BUILD_DIR)/test_drift.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Compile drift.c
$(BUILD_DIR)/drift.o: $(SRC_DIR)/drift.c $(INC_DIR)/drift.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# Compile main.c
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c $(INC_DIR)/drift.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# Compile test_drift.c
$(BUILD_DIR)/test_drift.o: $(TEST_DIR)/test_drift.c $(INC_DIR)/drift.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Run demo
demo: $(DEMO)
	@echo ""
	@echo "Running Drift Demo..."
	@echo "====================="
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

# Fixed-point build (for embedded targets)
fixed: CFLAGS += -DDRIFT_FIXED_POINT
fixed: clean all
