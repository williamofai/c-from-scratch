#!/bin/bash
#==============================================================================
# scaffold.sh - Create the c-from-scratch project structure
#==============================================================================
# 
# Usage: ./scaffold.sh [directory]
#   directory: Optional target directory (default: current directory)
#
# Creates a complete project scaffold for the c-from-scratch course.
#
# Copyright (c) 2025 William Murray
# MIT License - https://github.com/williamofai/c-from-scratch
#
#==============================================================================

set -e  # Exit on error

# Configuration
PROJECT_DIR="${1:-.}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Helper functions
info() { echo -e "${BLUE}[INFO]${NC} $1"; }
success() { echo -e "${GREEN}[OK]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1" >&2; exit 1; }

#------------------------------------------------------------------------------
# Create directory structure
#------------------------------------------------------------------------------
create_directories() {
    info "Creating directory structure..."
    
    mkdir -p "$PROJECT_DIR"/projects/pulse/{lessons,src,include,tests}
    mkdir -p "$PROJECT_DIR"/projects/pulse/lessons/01-the-problem
    mkdir -p "$PROJECT_DIR"/projects/pulse/lessons/02-mathematical-closure
    mkdir -p "$PROJECT_DIR"/projects/pulse/lessons/03-structs
    mkdir -p "$PROJECT_DIR"/projects/pulse/lessons/04-code
    mkdir -p "$PROJECT_DIR"/projects/pulse/lessons/05-testing
    
    success "Directory structure created"
}

#------------------------------------------------------------------------------
# Create root README.md
#------------------------------------------------------------------------------
create_root_readme() {
    info "Creating README.md..."
    
    cat > "$PROJECT_DIR/README.md" << 'EOF'
# C-From-Scratch

**Learn to build safety-critical systems in C.**

Not "Hello World". Real kernels. Mathematical rigour. Zero dependencies.

> *"Don't learn to code. Learn to prove, then transcribe."*

## Philosophy

Most tutorials teach you to write code that *seems* to work.  
This course teaches you to write code that *provably* works.

**The method:**
1. Define the problem precisely
2. Prove correctness mathematically  
3. Design structs that embody the proof
4. Code (it writes itself)
5. Test against the contracts

```
┌─────────────────────────────────────────────────────────────┐
│                    THE APPROACH                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   Problem  ──►  Math Model  ──►  Proof  ──►  Structs       │
│                                                │            │
│                                                ▼            │
│                           Verification  ◄──  Code          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## Projects

### [Pulse](./projects/pulse/) — Heartbeat Liveness Monitor
A tiny, provably-correct state machine that answers: *"Is this process alive?"*

- 5 lessons from problem definition to hardened code
- ~200 lines of C with mathematical proofs
- Handles clock wrap, faults, and edge cases
- Zero dependencies beyond libc

*More projects coming...*

## Who This Is For

- Developers who want to understand *why* code works, not just *that* it works
- Systems programmers building safety-critical software
- Anyone tired of "it works on my machine" engineering
- Students who want to learn C the right way

## Prerequisites

- Basic C syntax (variables, functions, structs)
- Comfort with command line
- Willingness to think before coding

## Getting Started

```bash
cd projects/pulse
make
./build/pulse
```

Then start with [Lesson 1: The Problem](./projects/pulse/lessons/01-the-problem/LESSON.md).

## Author

**William Murray** — 30 years UNIX systems engineering

- GitHub: [@williamofai](https://github.com/williamofai)
- LinkedIn: [William Murray](https://www.linkedin.com/in/williammurray1967/)
- Website: [speytech.com](https://speytech.com)

## License

MIT — See [LICENSE](./LICENSE)
EOF
    
    success "README.md created"
}

#------------------------------------------------------------------------------
# Create PHILOSOPHY.md
#------------------------------------------------------------------------------
create_philosophy() {
    info "Creating PHILOSOPHY.md..."
    
    cat > "$PROJECT_DIR/PHILOSOPHY.md" << 'EOF'
# Philosophy: Math → Structs → Code

## The Core Idea

Most programming tutorials teach you to:
1. Write code
2. Test code
3. Debug code
4. Hope it works

We do the opposite:
1. **Define the problem** precisely
2. **Prove the solution** mathematically
3. **Design the data** with explicit constraints
4. **Transcribe to code** (it writes itself)
5. **Verify the code** matches the math

## Why This Works

### Traditional Approach
```
Vague idea → Code → Bugs → Patches → More bugs → "Ship it"
```

### Our Approach
```
Problem → Math Model → Proof → Data Design → Code → Verification
    │         │          │          │          │          │
    │         │          │          │          │          └── Does code match math?
    │         │          │          │          └── Direct transcription
    │         │          │          └── Every field justified
    │         │          └── Contracts proven
    │         └── State machine defined
    └── Failure modes analysed
```

## The Three Pillars

### 1. Mathematical Closure

A system is **closed** if every possible input leads to exactly one defined output.

- **Closed**: Traffic light with defined transitions
- **Not closed**: `if (x > 0) return 1;` — what about x ≤ 0?

Our state machines are closed:
- Every state × every input = defined transition
- No undefined behaviour possible
- No "what if?" scenarios unconsidered

### 2. Contracts as Specifications

Instead of vague requirements like "should work well", we have formal contracts:

- **SOUNDNESS**: Never false positives
- **LIVENESS**: Always detect failure eventually
- **STABILITY**: No spurious transitions

Each contract is:
- Mathematically defined
- Proven to hold
- Tested in implementation

### 3. Data-Driven Design

Every struct field traces to a mathematical requirement:

| Field | Why It Exists |
|-------|---------------|
| `st` | Represents state space S |
| `t_init` | Required for init window W |
| `last_hb` | Required for timeout T |
| `have_hb` | Distinguishes UNKNOWN from DEAD |
| `fault_*` | Fail-safe contract enforcement |

No field exists "just in case" or "might be useful".

## What You'll Learn

After this course, you'll be able to:

1. **Analyse failure modes** before writing code
2. **Define state machines** that cover all cases
3. **Prove properties** about your design
4. **Design data structures** where every field is justified
5. **Write code** that's obviously correct
6. **Test systematically** against contracts

## The Payoff

Code written this way:
- Has fewer bugs (proven design)
- Is easier to modify (clear contracts)
- Is easier to review (math matches code)
- Survives corner cases (all cases considered)
- Documents itself (contracts as comments)

## A Warning

This approach takes more time upfront. You'll spend hours thinking before writing a line of code.

But you'll spend near-zero time debugging.

The total time is less. The quality is higher. The stress is lower.

---

*"Give me six hours to chop down a tree and I will spend the first four sharpening the axe."* — Abraham Lincoln

---

*"Don't learn to code. Learn to prove, then transcribe."*
EOF
    
    success "PHILOSOPHY.md created"
}

#------------------------------------------------------------------------------
# Create CONTRIBUTING.md
#------------------------------------------------------------------------------
create_contributing() {
    info "Creating CONTRIBUTING.md..."
    
    cat > "$PROJECT_DIR/CONTRIBUTING.md" << 'EOF'
# Contributing to C-From-Scratch

Thank you for your interest in contributing!

## Philosophy

This is an educational repository. Contributions should:

1. **Maintain rigour** — No hand-waving. Proofs matter.
2. **Preserve clarity** — Beginner-friendly explanations
3. **Follow the method** — Math → Structs → Code

## What We Need

### High Priority
- Typo fixes and clarifications
- Additional exercises
- Test cases for edge conditions
- Translations (if you're fluent)

### Medium Priority
- Additional projects following the same methodology
- Improved diagrams and visualisations
- Platform-specific build instructions

### Discussions Welcome
- Alternative approaches to proofs
- Additional failure mode analysis
- Real-world war stories

## How to Contribute

### Simple Fixes
1. Fork the repository
2. Make your change
3. Submit a pull request with clear description

### Larger Changes
1. Open an issue first to discuss
2. Wait for feedback before investing time
3. Follow the existing style and structure

## Style Guidelines

### Markdown
- One sentence per line (easier diffs)
- Use ATX headers (`#`, `##`, etc.)
- Code blocks with language specifier

### C Code
- C99 standard
- `snake_case` for functions and variables
- `UPPER_CASE` for constants
- Comments explain *why*, not *what*

### Commit Messages
- Present tense ("Add feature" not "Added feature")
- First line under 50 characters
- Reference issues when applicable

## Code of Conduct

Be kind. Be helpful. Assume good intent.

This is a learning environment. Questions are welcome, no matter how basic.

## Questions?

Open an issue with the "question" label.
EOF
    
    success "CONTRIBUTING.md created"
}

#------------------------------------------------------------------------------
# Create LICENSE
#------------------------------------------------------------------------------
create_license() {
    info "Creating LICENSE..."
    
    cat > "$PROJECT_DIR/LICENSE" << 'EOF'
MIT License

Copyright (c) 2025 William Murray

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
EOF
    
    success "LICENSE created"
}

#------------------------------------------------------------------------------
# Create .gitignore
#------------------------------------------------------------------------------
create_gitignore() {
    info "Creating .gitignore..."
    
    cat > "$PROJECT_DIR/.gitignore" << 'EOF'
# Build artifacts
*.o
*.a
*.so
*.exe
bin/
build/

# Editor
*.swp
*.swo
*~
.vscode/
.idea/

# OS
.DS_Store
Thumbs.db

# Debug
core
*.dSYM/

# Test artifacts
*.gcov
*.gcda
*.gcno
coverage/
EOF
    
    success ".gitignore created"
}

#------------------------------------------------------------------------------
# Create Pulse project README
#------------------------------------------------------------------------------
create_pulse_readme() {
    info "Creating projects/pulse/README.md..."
    
    cat > "$PROJECT_DIR/projects/pulse/README.md" << 'EOF'
# Pulse — Heartbeat Liveness Monitor

A tiny, provably-correct state machine that answers one question:

> **"Is this process alive?"**

## Overview

Pulse is a heartbeat-based liveness monitor built with mathematical rigour:

- **~200 lines of C** — Small enough to audit completely
- **Zero dependencies** — Just libc
- **Mathematically proven** — Contracts verified before code written
- **Handles edge cases** — Clock wrap, jumps, faults, reentry

## The Contracts

| Contract | Guarantee |
|----------|-----------|
| **Soundness** | Never report ALIVE if actually dead |
| **Liveness** | Eventually report DEAD if heartbeats stop |
| **Stability** | No spurious state transitions |

## Course Structure

| Lesson | Title | You Will Learn |
|--------|-------|----------------|
| 1 | [The Problem](lessons/01-the-problem/LESSON.md) | Why this matters, failure analysis |
| 2 | [Mathematical Closure](lessons/02-mathematical-closure/LESSON.md) | Prove correctness before coding |
| 3 | [Structs & Data Dictionary](lessons/03-structs/LESSON.md) | Design data with purpose |
| 4 | [Code](lessons/04-code/LESSON.md) | Transcribe math to C |
| 5 | [Testing & Hardening](lessons/05-testing/LESSON.md) | Verify and bulletproof |

## Quick Start

```bash
make
./build/pulse
```

## Building

```bash
# Standard build
make

# Run tests
make test

# Clean
make clean

# Static analysis (requires cppcheck)
make check
```

## Files

```
pulse/
├── README.md           # This file
├── Makefile            # Build system
├── lessons/            # Course content
│   ├── 01-the-problem/
│   ├── 02-mathematical-closure/
│   ├── 03-structs/
│   ├── 04-code/
│   └── 05-testing/
├── include/            # Header files
│   └── pulse.h
├── src/                # Implementation
│   ├── pulse.c
│   └── main.c
└── tests/              # Test suite
```

## Usage Example

```c
#include "pulse.h"

hb_fsm_t monitor;
uint64_t T = 5000;  // 5 second timeout
uint64_t W = 1000;  // 1 second init window

// Initialize
hb_init(&monitor, now_ms());

// In your main loop
while (running) {
    uint8_t heartbeat_received = check_for_heartbeat();
    hb_step(&monitor, now_ms(), heartbeat_received, T, W);
    
    switch (hb_state(&monitor)) {
        case STATE_UNKNOWN: /* Waiting for first heartbeat */ break;
        case STATE_ALIVE:   /* Process is healthy */ break;
        case STATE_DEAD:    /* Process died or fault detected */ break;
    }
}
```

## Why Not Just Use systemd/monit/etc?

See [Lesson 1](lessons/01-the-problem/LESSON.md) for a detailed analysis.

**TL;DR:** They're either too complex to verify or too simple to be correct.
EOF
    
    success "projects/pulse/README.md created"
}

#------------------------------------------------------------------------------
# Create Pulse Makefile
#------------------------------------------------------------------------------
create_pulse_makefile() {
    info "Creating projects/pulse/Makefile..."
    
    cat > "$PROJECT_DIR/projects/pulse/Makefile" << 'EOF'
# Makefile for pulse - Heartbeat Liveness Monitor
#
# Copyright (c) 2025 William Murray
# MIT License - https://github.com/williamofai/c-from-scratch
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
EOF
    
    success "projects/pulse/Makefile created"
}

#------------------------------------------------------------------------------
# Create source files
#------------------------------------------------------------------------------
create_src_files() {
    info "Creating source files..."
    
    # pulse.h in include/
    cat > "$PROJECT_DIR/projects/pulse/include/pulse.h" << 'EOF'
/**
 * pulse.h - Heartbeat-Based Liveness Monitor
 * 
 * A closed, total, deterministic state machine for monitoring
 * process liveness via heartbeat signals.
 * 
 * CONTRACTS:
 *   1. SOUNDNESS:  Never report ALIVE if actually dead
 *   2. LIVENESS:   Eventually report DEAD if heartbeats stop
 *   3. STABILITY:  No spurious transitions
 * 
 * REQUIREMENTS:
 *   - Single-writer access (caller must ensure)
 *   - Monotonic time source (caller provides)
 *   - Polling at bounded intervals (caller ensures)
 * 
 * See: lessons/02-mathematical-closure/LESSON.md for proofs
 *      lessons/03-structs/LESSON.md for data dictionary
 * 
 * Copyright (c) 2025 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#ifndef PULSE_H
#define PULSE_H

#include <stdint.h>

/**
 * Visible states of the liveness monitor.
 * 
 * Zero-initialisation yields STATE_UNKNOWN (safe default).
 */
typedef enum {
    STATE_UNKNOWN = 0,  /* No evidence yet                    */
    STATE_ALIVE   = 1,  /* Recent heartbeat observed          */
    STATE_DEAD    = 2   /* Timeout expired or fault detected  */
} state_t;

/**
 * Heartbeat Finite State Machine structure.
 * 
 * INVARIANTS:
 *   INV-1: st ∈ { UNKNOWN, ALIVE, DEAD }
 *   INV-2: (st == ALIVE) → (have_hb == 1)
 *   INV-3: (fault_time ∨ fault_reentry) → (st == DEAD)
 *   INV-4: (in_step == 0) when not executing hb_step
 */
typedef struct {
    state_t  st;            /* Current state ∈ S                    */
    uint64_t t_init;        /* Boot/reset reference time            */
    uint64_t last_hb;       /* Timestamp of most recent heartbeat   */
    uint8_t  have_hb;       /* Evidence flag: ≥1 heartbeat observed */
    uint8_t  fault_time;    /* Fault: clock corruption detected     */
    uint8_t  fault_reentry; /* Fault: atomicity violation detected  */
    uint8_t  in_step;       /* Reentrancy guard                     */
} hb_fsm_t;

/**
 * Initialise the state machine.
 * 
 * @param m   Pointer to state machine structure
 * @param now Current timestamp from monotonic source
 */
void hb_init(hb_fsm_t *m, uint64_t now);

/**
 * Execute one atomic step of the state machine.
 * 
 * @param m       Pointer to initialised state machine
 * @param now     Current timestamp
 * @param hb_seen 1 if heartbeat observed this step, 0 otherwise
 * @param T       Timeout threshold (time units)
 * @param W       Initialisation window (time units)
 */
void hb_step(hb_fsm_t *m, uint64_t now, uint8_t hb_seen,
             uint64_t T, uint64_t W);

/** Query current state. */
static inline state_t hb_state(const hb_fsm_t *m) {
    return m->st;
}

/** Check if any fault has been detected. */
static inline uint8_t hb_faulted(const hb_fsm_t *m) {
    return m->fault_time || m->fault_reentry;
}

/** Check if evidence has ever been observed. */
static inline uint8_t hb_has_evidence(const hb_fsm_t *m) {
    return m->have_hb;
}

#endif /* PULSE_H */
EOF
    
    # pulse.c
    cat > "$PROJECT_DIR/projects/pulse/src/pulse.c" << 'EOF'
/**
 * pulse.c - Heartbeat-Based Liveness Monitor Implementation
 * 
 * This is a direct transcription of the mathematical model.
 * Every line traces to a contract or transition table entry.
 * 
 * See: lessons/02-mathematical-closure/LESSON.md
 * 
 * Copyright (c) 2025 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#include "pulse.h"

/*---------------------------------------------------------------------------
 * Helper Functions
 *---------------------------------------------------------------------------*/

/** Modular age computation: (now - then) mod 2^64 */
static inline uint64_t age_u64(uint64_t now, uint64_t then)
{
    return (uint64_t)(now - then);
}

/** Half-range rule: valid if age < 2^63 */
static inline uint8_t age_valid(uint64_t age)
{
    return (age < (1ULL << 63));
}

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

void hb_init(hb_fsm_t *m, uint64_t now)
{
    m->st = STATE_UNKNOWN;
    m->t_init = now;
    m->last_hb = 0;
    m->have_hb = 0;
    m->fault_time = 0;
    m->fault_reentry = 0;
    m->in_step = 0;
}

void hb_step(hb_fsm_t *m, uint64_t now, uint8_t hb_seen,
             uint64_t T, uint64_t W)
{
    /* Reentrancy check — CONTRACT enforcement */
    if (m->in_step) {
        m->fault_reentry = 1;
        m->st = STATE_DEAD;
        return;
    }
    m->in_step = 1;

    /* Record heartbeat if seen */
    if (hb_seen) {
        m->last_hb = now;
        m->have_hb = 1;
    }

    /* No evidence yet — stay UNKNOWN */
    if (!m->have_hb) {
        uint64_t a_init = age_u64(now, m->t_init);
        if (!age_valid(a_init)) {
            m->fault_time = 1;
            m->st = STATE_DEAD;
        } else {
            m->st = STATE_UNKNOWN;
            /* Note: W (init window) not used here since we have no evidence.
             * We stay UNKNOWN regardless of how long we've waited. */
            (void)W;  /* Suppress unused parameter warning */
        }
        m->in_step = 0;
        return;
    }

    /* Have evidence — check age */
    uint64_t a_hb = age_u64(now, m->last_hb);
    if (!age_valid(a_hb)) {
        m->fault_time = 1;
        m->st = STATE_DEAD;
        m->in_step = 0;
        return;
    }

    /* Transition based on timeout — direct from transition table */
    m->st = (a_hb > T) ? STATE_DEAD : STATE_ALIVE;
    m->in_step = 0;
}
EOF
    
    # main.c
    cat > "$PROJECT_DIR/projects/pulse/src/main.c" << 'EOF'
/**
 * main.c - Example usage of pulse liveness monitor
 * 
 * This demonstrates basic usage of the pulse API.
 * In production, you would integrate with your actual
 * heartbeat source (pipes, signals, sockets, etc.)
 * 
 * Copyright (c) 2025 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#define _XOPEN_SOURCE 500  /* For usleep */

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "pulse.h"

/** Get current time in milliseconds (monotonic) */
static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + 
           (uint64_t)ts.tv_nsec / 1000000ULL;
}

/** Convert state to string for display */
static const char* state_name(state_t st)
{
    switch (st) {
        case STATE_UNKNOWN: return "UNKNOWN";
        case STATE_ALIVE:   return "ALIVE";
        case STATE_DEAD:    return "DEAD";
        default:            return "INVALID";
    }
}

int main(void)
{
    hb_fsm_t monitor;
    uint64_t T = 2000;  /* 2 second timeout */
    uint64_t W = 500;   /* 0.5 second init window */
    
    printf("pulse - Heartbeat Liveness Monitor Demo\n");
    printf("========================================\n");
    printf("Timeout (T): %lu ms\n", (unsigned long)T);
    printf("Init window (W): %lu ms\n\n", (unsigned long)W);
    
    /* Initialize */
    hb_init(&monitor, now_ms());
    printf("[%8lu] Initialized: state = %s\n", 
           (unsigned long)now_ms(), state_name(hb_state(&monitor)));
    
    /* Simulate: heartbeat every 500ms for 5 beats, then stop */
    printf("\n--- Sending heartbeats every 500ms ---\n");
    for (int i = 0; i < 5; i++) {
        usleep(500000);  /* 500ms */
        hb_step(&monitor, now_ms(), 1, T, W);  /* heartbeat seen */
        printf("[%8lu] Heartbeat #%d: state = %s\n", 
               (unsigned long)now_ms(), i + 1, 
               state_name(hb_state(&monitor)));
    }
    
    /* Simulate: no heartbeats, watch timeout */
    printf("\n--- Stopping heartbeats, watching for timeout ---\n");
    for (int i = 0; i < 6; i++) {
        usleep(500000);  /* 500ms */
        hb_step(&monitor, now_ms(), 0, T, W);  /* no heartbeat */
        printf("[%8lu] No heartbeat: state = %s%s\n", 
               (unsigned long)now_ms(),
               state_name(hb_state(&monitor)),
               hb_faulted(&monitor) ? " [FAULT]" : "");
        
        if (hb_state(&monitor) == STATE_DEAD) {
            printf("\n*** Process declared DEAD after timeout ***\n");
            break;
        }
    }
    
    /* Simulate: recovery */
    printf("\n--- Heartbeat resumes (recovery) ---\n");
    usleep(500000);
    hb_step(&monitor, now_ms(), 1, T, W);  /* heartbeat seen */
    printf("[%8lu] Heartbeat received: state = %s\n", 
           (unsigned long)now_ms(), state_name(hb_state(&monitor)));
    
    printf("\nDemo complete.\n");
    return 0;
}
EOF
    
    success "Source files created"
}

#------------------------------------------------------------------------------
# Main execution
#------------------------------------------------------------------------------
main() {
    echo ""
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║         C-From-Scratch — Project Scaffold Generator            ║"
    echo "║                                                                ║"
    echo "║  Copyright (c) 2025 William Murray                             ║"
    echo "║  https://github.com/williamofai/c-from-scratch                 ║"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo ""
    
    # Create everything
    create_directories
    create_root_readme
    create_philosophy
    create_contributing
    create_license
    create_gitignore
    create_pulse_readme
    create_pulse_makefile
    create_src_files
    
    echo ""
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║                    Scaffold Complete!                          ║"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo ""
    info "Project created in: $PROJECT_DIR/"
    echo ""
    echo "Next steps:"
    echo "  1. Copy lesson files to projects/pulse/lessons/"
    echo "  2. cd projects/pulse && make"
    echo "  3. ./build/pulse"
    echo ""
    echo "Structure:"
    find "$PROJECT_DIR" -type f \( -name "*.md" -o -name "*.c" -o -name "*.h" -o -name "Makefile" \) 2>/dev/null | sort | head -20 | sed 's/^/  /'
    echo ""
}

# Run
main "$@"
