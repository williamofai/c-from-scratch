# Baseline - Statistical Normality Monitor
# Part of c-from-scratch
#
# Usage:
#   make          - Build demo and tests
#   make demo     - Build and run demo
#   make test     - Build and run tests
#   make clean    - Remove build artifacts
#   make check    - Run static analysis

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

# Source files
SRCS = $(SRC_DIR)/baseline.c
MAIN_SRC = $(SRC_DIR)/main.c
TEST_SRC = $(TEST_DIR)/test_baseline.c

# Object files
OBJS = $(BUILD_DIR)/baseline.o $(BUILD_DIR)/main.o
TEST_OBJS = $(BUILD_DIR)/baseline.o $(BUILD_DIR)/test_baseline.o

# Targets
DEMO = $(BUILD_DIR)/baseline
TEST = $(BUILD_DIR)/test_contracts

# Libraries
LIBS = -lm

.PHONY: all clean demo test check

all: $(DEMO) $(TEST)

# Build demo executable
$(DEMO): $(BUILD_DIR)/baseline.o $(BUILD_DIR)/main.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Build test executable
$(TEST): $(BUILD_DIR)/baseline.o $(BUILD_DIR)/test_baseline.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Compile baseline.c
$(BUILD_DIR)/baseline.o: $(SRC_DIR)/baseline.c $(INC_DIR)/baseline.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# Compile main.c
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c $(INC_DIR)/baseline.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# Compile test_baseline.c
$(BUILD_DIR)/test_baseline.o: $(TEST_DIR)/test_baseline.c $(INC_DIR)/baseline.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Run demo
demo: $(DEMO)
	@echo ""
	@echo "Running Baseline Demo..."
	@echo "========================"
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
