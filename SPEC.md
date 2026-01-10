# c-from-scratch Framework Specification

**Version 1.1.0**  
**January 2026**

> *"Math → Structs → Code"*

---

## Abstract

c-from-scratch is a framework for teaching safety-critical C programming through mathematically-proven, deterministic state machines. Each module answers one question about a system's health, and all modules compose into complete monitoring pipelines.

This specification defines the core principles, module structure, contracts, and composition rules that govern the framework.

---

## Table of Contents

1. [Philosophy](#1-philosophy)
2. [Core Principles](#2-core-principles)
3. [Module Structure](#3-module-structure)
4. [The Seven Foundation Modules](#4-the-seven-foundation-modules)
5. [Contracts & Invariants](#5-contracts--invariants)
6. [Composition Rules](#6-composition-rules)
7. [Code Standards](#7-code-standards)
8. [Testing Requirements](#8-testing-requirements)
9. [Lesson Structure](#9-lesson-structure)
10. [Certification Alignment](#10-certification-alignment)

---

## 1. Philosophy

### 1.1 The Problem

Most C programming education teaches syntax without rigor. Students learn `for` loops but not formal contracts. They write code that works but cannot prove *why* it works.

Safety-critical systems demand more. Aviation (DO-178C), medical devices (IEC 62304), and automotive (ISO 26262) require:
- Deterministic behaviour
- Bounded resource usage
- Provable correctness
- Traceable requirements

### 1.2 The Solution

c-from-scratch teaches C through the lens of safety-critical development:

1. **Math First**: Define the problem mathematically before writing code
2. **Structs Second**: Encode the math in data structures
3. **Code Third**: Transcribe the math into C — the code *is* the proof

### 1.3 The Insight

> "A state machine that is closed, total, and deterministic can be formally verified."

- **Closed**: No external dependencies at runtime
- **Total**: Handles all possible inputs (no undefined behaviour)
- **Deterministic**: Same inputs always produce same outputs

---

## 2. Core Principles

### 2.1 O(1) Everything

All operations must be constant-time and constant-space:

| Property | Requirement |
|----------|-------------|
| Time complexity | O(1) per update |
| Space complexity | O(1) fixed state |
| No dynamic allocation | `malloc` forbidden |
| No recursion | Stack depth bounded |

### 2.2 Init-Update-Query Pattern

Every module follows the same API pattern:

```c
int  module_init(module_t *m, const config_t *cfg);  // Initialise
int  module_update(module_t *m, input_t in, result_t *out);  // Step
state_t module_state(const module_t *m);  // Query (inline)
void module_reset(module_t *m);  // Reset to initial state
```

### 2.3 Zero-Initialisation Safety

```c
module_t m = {0};  // Must be safe (INIT or LEARNING state)
```

### 2.4 Error Handling

Functions return error codes, never crash:

```c
typedef enum {
    MODULE_OK           =  0,
    MODULE_ERR_NULL     = -1,
    MODULE_ERR_CONFIG   = -2,
    MODULE_ERR_DOMAIN   = -3,
    MODULE_ERR_STATE    = -4,
    // ...
} module_error_t;
```

### 2.5 No Global State

All state lives in the module struct. Multiple instances are independent.

---

## 3. Module Structure

### 3.1 Directory Layout

```
module-name/
├── include/
│   └── module.h          # Public API, contracts, types
├── src/
│   ├── module.c          # Implementation
│   └── main.c            # Demo program
├── tests/
│   └── test_module.c     # Contract test suite
├── lessons/
│   ├── 01-the-problem/
│   │   └── LESSON.md
│   ├── 02-mathematical-model/
│   │   └── LESSON.md
│   ├── 03-structs/
│   │   └── LESSON.md
│   ├── 04-code/
│   │   └── LESSON.md
│   ├── 05-testing/
│   │   └── LESSON.md
│   └── 06-composition/
│       └── LESSON.md
├── Makefile
├── README.md
└── .gitignore
```

### 3.2 Header File Structure

```c
/**
 * module.h - One-line description
 * 
 * Extended description of what this module does.
 * 
 * CONTRACTS:
 *   1. CONTRACT-1: Description
 *   2. CONTRACT-2: Description
 * 
 * Copyright (c) 2026 Author
 * MIT License
 */

#ifndef MODULE_H
#define MODULE_H

/* Error codes */
typedef enum { ... } module_error_t;

/* FSM states */
typedef enum { ... } module_state_t;

/* Configuration (immutable after init) */
typedef struct { ... } module_config_t;

/* Result structure */
typedef struct { ... } module_result_t;

/* FSM structure */
typedef struct { ... } module_t;

/* Public API */
int module_init(...);
int module_update(...);
void module_reset(...);

/* Inline queries */
static inline module_state_t module_state(...);

#endif
```

---

## 4. The Seven Foundation Modules

### 4.1 Module Overview

| # | Module | Question | Role | Output |
|---|--------|----------|------|--------|
| 1 | Pulse | Does it exist? | Sensor | ALIVE / DEAD |
| 2 | Baseline | Is it normal? | Sensor | STABLE / DEVIATION |
| 3 | Timing | Is it regular? | Sensor | HEALTHY / UNHEALTHY |
| 4 | Drift | Is it trending toward failure? | Sensor | STABLE / DRIFTING |
| 5 | Consensus | Which sensor to trust? | Judge | Voted value + confidence |
| 6 | Pressure | How to handle overflow? | Buffer | Bounded queue + backpressure |
| 7 | Mode | What do we do about it? | Captain | System mode + permissions |

### 4.2 Module 1: Pulse (Heartbeat Monitor)

**Question**: "Is the signal source alive?"

**Contract**: If no heartbeat received within timeout T, declare DEAD.

**States**: INIT → ALIVE ↔ DEAD

**Key insight**: Existence must be proven continuously.

### 4.3 Module 2: Baseline (Normality Detector)

**Question**: "Is the value within normal range?"

**Contract**: If |value - baseline| > threshold after learning period, declare DEVIATION.

**States**: INIT → LEARNING → STABLE ↔ DEVIATION

**Key insight**: "Normal" must be learned, not assumed.

### 4.4 Module 3: Timing (Regularity Monitor)

**Question**: "Are events arriving on schedule?"

**Contract**: If |actual_interval - expected_interval| > tolerance, declare UNHEALTHY.

**States**: INIT → HEALTHY ↔ UNHEALTHY

**Key insight**: Timing jitter indicates system stress.

### 4.5 Module 4: Drift (Trend Detector)

**Question**: "Is the value moving toward failure?"

**Contract**: If |slope| > max_safe_slope, declare DRIFTING.

**Algorithm**: Damped derivative via EMA of instantaneous slope.

**States**: LEARNING → STABLE ↔ DRIFTING_UP / DRIFTING_DOWN → FAULT

**Key insight**: "Temperature is normal but rising too fast."

### 4.6 Module 5: Consensus (TMR Voter)

**Question**: "Which of three sensors should we trust?"

**Contract**: One faulty sensor cannot corrupt the output (single-fault tolerance).

**Algorithm**: Mid-Value Selection (median of 3).

**States**: INIT → AGREE / DISAGREE / DEGRADED / NO_QUORUM → FAULT

**Key insight**: "With THREE clocks, we can outvote the liar."

### 4.7 Module 6: Pressure (Bounded Queue)

**Question**: "What do we do when messages arrive faster than we can process?"

**Contract**: Queue never exceeds configured capacity; all items tracked.

**Policies**: REJECT (backpressure), DROP_OLDEST (lossy), DROP_NEWEST (preserve history)

**States**: LOW → NORMAL → HIGH → CRITICAL

**Key insight**: "Only bounded queues let you choose deliberately."

### 4.8 Module 7: Mode (System Orchestrator)

**Question**: "Given all health signals, what should the system DO?"

**Contract**: System exists in exactly one mode; OPERATIONAL requires all healthy; EMERGENCY is sticky.

**Modes**: INIT → STARTUP → OPERATIONAL ↔ DEGRADED → EMERGENCY (+ TEST)

**Features**:
- Permissions matrix constrains actions per mode
- Value-aware: semantic flags enable proactive degradation
- Audit log: all transitions logged with timestamp and cause
- Hysteresis: minimum dwell time prevents mode flapping

**Key insight**: "Sensors report. The Captain decides."

---

## 5. Contracts & Invariants

### 5.1 Contract Definition

A **contract** is a guarantee the module makes to its caller:

```
CONTRACT-N: If preconditions P hold, then postconditions Q hold.
```

Contracts are:
- Documented in the header file
- Tested explicitly in the test suite
- Never violated by the implementation

### 5.2 Invariant Definition

An **invariant** is a property that always holds:

```
INV-N: Property P is true in all reachable states.
```

Invariants are:
- Verified by assertions (debug builds)
- Checked in fuzz tests
- Part of the formal model

### 5.3 Example: Mode Manager Contracts

```
CONTRACT-1 (Unambiguous State):
  System exists in exactly one mode at any time.

CONTRACT-2 (Safe Entry):
  OPERATIONAL mode requires all monitors HEALTHY.

CONTRACT-3 (Fault Stickiness):
  EMERGENCY mode requires explicit reset to exit.

CONTRACT-4 (No Skip):
  Cannot transition directly from INIT to OPERATIONAL.

CONTRACT-5 (Bounded Latency):
  Fault detected → EMERGENCY in ≤ 1 cycle.

CONTRACT-6 (Deterministic):
  Same inputs → Same mode, always.

CONTRACT-7 (Proactive Safety):
  Critical flags trigger DEGRADED before actual faults.

CONTRACT-8 (Auditability):
  All transitions logged with timestamp and cause.
```

---

## 6. Composition Rules

### 6.1 The Safety Stack

```
┌─────────────────────────────────────────────────────────────────────┐
│                    MODULE 7: MODE MANAGER                           │
│                       "The Captain"                                 │
│   Decides: What mode? What actions allowed?                         │
└─────────────────────────────────────────────────────────────────────┘
                              ↑
┌─────────────────────────────────────────────────────────────────────┐
│                    MODULE 6: PRESSURE                               │
│                       "The Buffer"                                  │
│   Handles: Message overflow, backpressure                           │
└─────────────────────────────────────────────────────────────────────┘
                              ↑
┌─────────────────────────────────────────────────────────────────────┐
│                    MODULE 5: CONSENSUS                              │
│                       "The Judge"                                   │
│   Decides: Which sensor to trust?                                   │
└─────────────────────────────────────────────────────────────────────┘
                              ↑
          ┌───────────────────┼───────────────────┐
          ▼                   ▼                   ▼
┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
│   CHANNEL 0     │ │   CHANNEL 1     │ │   CHANNEL 2     │
│  Pulse → Base   │ │  Pulse → Base   │ │  Pulse → Base   │
│  → Timing       │ │  → Timing       │ │  → Timing       │
│  → Drift        │ │  → Drift        │ │  → Drift        │
│   "Sensors"     │ │   "Sensors"     │ │   "Sensors"     │
└─────────────────┘ └─────────────────┘ └─────────────────┘
```

### 6.2 Health State Propagation

Modules produce health states that feed downstream modules:

```
Pulse:     ALIVE → HEALTHY,  DEAD → FAULTY
Baseline:  STABLE → HEALTHY, DEVIATION → DEGRADED
Timing:    HEALTHY → HEALTHY, UNHEALTHY → DEGRADED
Drift:     STABLE → HEALTHY, DRIFTING → DEGRADED, FAULT → FAULTY
```

### 6.3 Value-Aware Flags

Modules can set semantic flags based on domain knowledge:

| Flag | Source | Meaning |
|------|--------|---------|
| approaching_upper | Drift | TTF < critical threshold |
| approaching_lower | Drift | TTF < critical threshold |
| low_confidence | Consensus | Confidence < 50% |
| queue_critical | Pressure | Fill > 90% |
| timing_unstable | Timing | Recent jitter violations |
| baseline_volatile | Baseline | High recent deviation |

These enable **proactive safety**: act before failure.

### 6.4 Independence

Each module instance is independent:
- No shared state between instances
- Can run different configurations
- Failure of one doesn't affect others

---

## 7. Code Standards

### 7.1 Compiler Flags

```makefile
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c11 -O2
```

### 7.2 Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Types | `snake_case_t` | `drift_state_t` |
| Functions | `module_action` | `drift_update` |
| Constants | `MODULE_CONSTANT` | `DRIFT_OK` |
| Macros | `MODULE_MACRO` | `DRIFT_TO_FLOAT` |

### 7.3 Forbidden Constructs

| Construct | Reason |
|-----------|--------|
| `malloc` / `free` | Non-deterministic timing |
| Recursion | Unbounded stack |
| `goto` | Unstructured control flow |
| Global variables | Hidden state |
| Floating-point (optional) | Non-deterministic on some hardware |

### 7.4 Required Constructs

| Construct | Purpose |
|-----------|---------|
| `static inline` | Zero-cost query functions |
| `const` parameters | Immutability guarantee |
| Designated initialisers | Clear struct initialisation |
| Explicit casts | No implicit conversions |

---

## 8. Testing Requirements

### 8.1 Test Categories

| Category | Purpose | Coverage |
|----------|---------|----------|
| Contract tests | Prove contracts hold | All contracts |
| Invariant tests | Verify invariants | All invariants |
| Edge case tests | Boundary conditions | NaN, overflow, empty |
| Fuzz tests | Random inputs | 10,000+ iterations |

### 8.2 Test Output Format

```
╔════════════════════════════════════════════════════════════════╗
║           MODULE Contract Test Suite                           ║
╚════════════════════════════════════════════════════════════════╝

Contract Tests:
  [PASS] CONTRACT-1: Description
  [PASS] CONTRACT-2: Description

Invariant Tests:
  [PASS] INV-1: Description

Fuzz Tests:
  [PASS] Fuzz: 10000 random inputs, invariants held

══════════════════════════════════════════════════════════════════
  Results: N/N tests passed
══════════════════════════════════════════════════════════════════
```

### 8.3 Coverage Requirements

- All contracts must have explicit tests
- All error paths must be exercised
- Fuzz testing must run without crashes

---

## 9. Lesson Structure

### 9.1 Six-Lesson Progression

| Lesson | Title | Content |
|--------|-------|---------|
| 01 | The Problem | Why this matters, real-world failures |
| 02 | Mathematical Model | Formal definitions, proofs |
| 03 | Structs | Data structure design, field justification |
| 04 | Code | Implementation as mathematical transcription |
| 05 | Testing | Contract verification, fuzz testing |
| 06 | Composition | Integration with other modules |

### 9.2 Lesson Format

Each lesson follows this structure:

```markdown
# c-from-scratch — Module N: Name

## Lesson X: Title

---

## Core Concept

> "Key insight in one sentence."

---

## Content sections...

---

## Bridge to Next Lesson

What comes next and why.
```

---

## 10. Certification Alignment

### 10.1 DO-178C (Aviation)

| DO-178C Objective | c-from-scratch Support |
|-------------------|------------------------|
| MC/DC coverage | Contract tests |
| Traceability | Contracts → Tests → Code |
| Determinism | O(1), no malloc |
| Bounded resources | Fixed-size state |

### 10.2 IEC 62304 (Medical)

| IEC 62304 Requirement | c-from-scratch Support |
|-----------------------|------------------------|
| Software architecture | Module composition |
| Unit verification | Contract test suite |
| Risk control | Defensive coding |

### 10.3 ISO 26262 (Automotive)

| ISO 26262 Requirement | c-from-scratch Support |
|-----------------------|------------------------|
| Freedom from interference | Independent modules |
| Fault tolerance | Consensus voting (TMR) |
| Diagnostic coverage | Health state propagation |

---

## Appendix A: Quick Reference

### Module API Template

```c
// Initialise
int module_init(module_t *m, const config_t *cfg);

// Update (step the FSM)
int module_update(module_t *m, input_t in, result_t *out);

// Query current state (inline, O(1))
static inline state_t module_state(const module_t *m);

// Reset to initial state
void module_reset(module_t *m);
```

### Error Code Template

```c
typedef enum {
    MODULE_OK           =  0,
    MODULE_ERR_NULL     = -1,
    MODULE_ERR_CONFIG   = -2,
    MODULE_ERR_DOMAIN   = -3,
    MODULE_ERR_STATE    = -4,
    MODULE_ERR_OVERFLOW = -5,
    MODULE_ERR_REENTRY  = -6,
    MODULE_ERR_FAULT    = -7
} module_error_t;
```

### FSM State Template

```c
typedef enum {
    STATE_INIT     = 0,  // Zero-init safe
    STATE_LEARNING = 1,
    STATE_STABLE   = 2,
    STATE_FAULT    = 3
} module_state_t;
```

---

## Appendix B: Glossary

| Term | Definition |
|------|------------|
| **Closed** | No external dependencies at runtime |
| **Contract** | Guarantee from module to caller |
| **Deterministic** | Same inputs → Same outputs |
| **EMA** | Exponential Moving Average |
| **FSM** | Finite State Machine |
| **Invariant** | Property that always holds |
| **MVS** | Mid-Value Selection (median) |
| **TMR** | Triple Modular Redundancy |
| **Total** | Handles all possible inputs |
| **TTF** | Time To Failure |

---

## Appendix C: Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | January 2026 | Initial specification (6 modules) |
| 1.1.0 | January 2026 | Added Module 7: Mode Manager |

---

## License

This specification is released under the MIT License.

Copyright (c) 2026 William Murray

---

*"Sensors report. The Captain decides."*
