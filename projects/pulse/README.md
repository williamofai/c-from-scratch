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
├── src/                # Implementation
│   ├── pulse.h         # Interface & contracts
│   ├── pulse.c         # State machine
│   └── main.c          # Example usage
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
