# Lesson 4: Code

## The Code Writes Itself

> "The code is just the math, wearing a different costume."

With the mathematical model proven (Lesson 2) and the data structures designed (Lesson 3), writing the code is straightforward **transcription**. Not invention. Not creativity. Just careful translation.

Every line traces to a contract. Every branch traces to the transition table.

---

## The Implementation Strategy

We're going to build this in four parts:

1. **Helper functions** — Age computation and validation
2. **Initialisation** — Setting up a clean, safe state
3. **The step function** — The core transition logic
4. **Inline accessors** — Zero-overhead state queries

Each part maps directly to mathematical definitions from Lesson 2.

---

## Part 1: Helper Functions

### Age Computation

From Lesson 2, we defined age as modular subtraction:

```
age(a, b) = (a - b) mod 2^N
```

In C:

```c
/** Modular age computation: (now - then) mod 2^64 */
static inline uint64_t age_u64(uint64_t now, uint64_t then)
{
    return (uint64_t)(now - then);
}
```

**Why this works:**

- `uint64_t` subtraction wraps at 2^64 (guaranteed by C standard)
- The cast to `uint64_t` is technically redundant but makes intent explicit
- `static inline` means zero function call overhead

**Why `static inline`?**

| Alternative | Problem |
|-------------|---------|
| Regular function | Function call overhead on every step |
| Macro | No type checking, multiple evaluation |
| `static inline` | Best of both: type-safe, zero overhead |

### Age Validation

From Lesson 2, the half-range rule:

```
age is valid iff age < 2^(N-1)
```

In C:

```c
/** Half-range rule: valid if age < 2^63 */
static inline uint8_t age_valid(uint64_t age)
{
    return (age < (1ULL << 63));
}
```

**Breaking down `(1ULL << 63)`:**

- `1ULL` — Unsigned long long literal (64-bit)
- `<< 63` — Shift left by 63 bits
- Result: 2^63 = 9,223,372,036,854,775,808

**Why return `uint8_t`?**

Boolean results as `uint8_t` for consistency with struct fields. Using `int` would work but wastes space and mixes types.

---

## Part 2: Initialisation

From Lesson 3, the postconditions for initialisation:

- `st = STATE_UNKNOWN` (safe default)
- `t_init = now` (record start time)
- All evidence and fault flags cleared

```c
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
```

**Why explicit assignment (not `memset`)?**

```c
// DON'T DO THIS
memset(m, 0, sizeof(*m));
m->t_init = now;
```

Problems with `memset`:
- Assumes `STATE_UNKNOWN == 0` (implementation detail leak)
- Assumes all-zeros is valid for all types (not always true)
- Less readable — intent unclear
- Compiler optimises explicit assignment anyway

**Why no return value?**

Initialisation can't fail (no allocation, no I/O). A void return makes this clear. If you need to validate parameters, do it in the caller.

---

## Part 3: The Step Function

This is the heart of the implementation — the transition function δ from Lesson 2.

### Structure Overview

```c
void hb_step(hb_fsm_t *m, uint64_t now, uint8_t hb_seen,
             uint64_t T, uint64_t W)
{
    // 1. Reentrancy check
    // 2. Record heartbeat if seen
    // 3. Handle no-evidence case
    // 4. Validate age
    // 5. Apply transition
}
```

### Step 1: Reentrancy Check

**Contract enforcement**: Detect concurrent access and fail safe.

```c
if (m->in_step) {
    m->fault_reentry = 1;
    m->st = STATE_DEAD;
    return;
}
m->in_step = 1;
```

**Trace to requirements:**
- INV-3: `fault_reentry → st == DEAD`
- Fail-safe principle: When in doubt, report DEAD

**Why return immediately?**

The struct may be in an inconsistent state. Don't touch anything else.

### Step 2: Record Heartbeat

```c
if (hb_seen) {
    m->last_hb = now;
    m->have_hb = 1;
}
```

**Order matters:**

1. Update `last_hb` first (the timestamp)
2. Then set `have_hb` (the evidence flag)

If interrupted between these, `have_hb = 0` with stale `last_hb` is safer than `have_hb = 1` with uninitialised `last_hb`.

### Step 3: Handle No Evidence

```c
if (!m->have_hb) {
    uint64_t a_init = age_u64(now, m->t_init);
    if (!age_valid(a_init)) {
        m->fault_time = 1;
        m->st = STATE_DEAD;
    } else {
        m->st = STATE_UNKNOWN;
    }
    m->in_step = 0;
    return;
}
```

**Trace to transition table (Lesson 2, Section 5.2):**

| Current | hb_seen | Condition | Next | Row |
|---------|---------|-----------|------|-----|
| UNKNOWN | 0 | a_init < W | UNKNOWN | 2 |
| UNKNOWN | 0 | a_init ≥ W | UNKNOWN | 3 |
| ANY | ANY | age invalid | DEAD | 1 |

When we have no evidence (`have_hb = 0`), we stay UNKNOWN regardless of how long we've waited. The W parameter was designed for future expansion but isn't used in this minimal implementation.

**Why check age validity here?**

Clock corruption could happen at any time. If `t_init` is corrupted or the clock jumped, we detect it early.

### Step 4: Validate Heartbeat Age

```c
uint64_t a_hb = age_u64(now, m->last_hb);
if (!age_valid(a_hb)) {
    m->fault_time = 1;
    m->st = STATE_DEAD;
    m->in_step = 0;
    return;
}
```

**Trace to transition table:**

| Current | hb_seen | Condition | Next | Row |
|---------|---------|-----------|------|-----|
| ANY | ANY | age invalid | DEAD | 1 |

This catches:
- Clock jumped backwards (making `last_hb` appear in the "future")
- Clock jumped forward by more than 2^63 time units
- Memory corruption of `last_hb`

### Step 5: Apply Transition

```c
m->st = (a_hb > T) ? STATE_DEAD : STATE_ALIVE;
m->in_step = 0;
```

**Trace to transition table:**

| Current | hb_seen | Condition | Next | Row |
|---------|---------|-----------|------|-----|
| ALIVE | 1 | a_hb ≤ T | ALIVE | 5 |
| ALIVE | 0 | a_hb > T | DEAD | 6 |
| DEAD | 1 | a_hb ≤ T | ALIVE | 7 |
| DEAD | 0 | a_hb > T | DEAD | 8 |

**Why a ternary instead of if/else?**

```c
// This:
m->st = (a_hb > T) ? STATE_DEAD : STATE_ALIVE;

// Is clearer than:
if (a_hb > T) {
    m->st = STATE_DEAD;
} else {
    m->st = STATE_ALIVE;
}
```

The ternary makes the binary nature of the decision obvious. Both compile to the same machine code.

**Why don't we check current state?**

Look at the transition table rows 5-8. The next state depends **only** on whether `a_hb > T`. The current state doesn't affect the outcome. This is elegant: the math told us we don't need a switch statement.

---

## Part 4: Inline Accessors

```c
static inline state_t hb_state(const hb_fsm_t *m) {
    return m->st;
}

static inline uint8_t hb_faulted(const hb_fsm_t *m) {
    return m->fault_time || m->fault_reentry;
}

static inline uint8_t hb_has_evidence(const hb_fsm_t *m) {
    return m->have_hb;
}
```

**Why accessors instead of direct field access?**

| Direct Access | Accessor |
|---------------|----------|
| `monitor.st` | `hb_state(&monitor)` |
| Exposes internal structure | Hides implementation |
| Can't add validation later | Can add checks without changing callers |
| No documentation point | Clear API boundary |

**Why `const hb_fsm_t *`?**

The `const` declares that these functions don't modify the struct. This:
- Documents intent
- Enables compiler optimisations
- Catches accidental modifications

---

## The Complete Implementation

```c
/**
 * pulse.c - Heartbeat-Based Liveness Monitor Implementation
 * 
 * This is a direct transcription of the mathematical model.
 * Every line traces to a contract or transition table entry.
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
```

**Line count**: ~60 lines of actual code (excluding comments and whitespace).

---

## Code-to-Contract Traceability

Every section of code maps to a mathematical requirement:

| Code Section | Contract/Requirement |
|--------------|---------------------|
| `age_u64()` | Time model (Lesson 2, §1.2) |
| `age_valid()` | Half-range rule (Lesson 2, §1.2) |
| `hb_init()` | Initial state postconditions (Lesson 3) |
| Reentrancy check | INV-3, fail-safe principle |
| Heartbeat recording | Evidence tracking |
| No-evidence branch | Transition table rows 1-3 |
| Age validation | Transition table row 1 |
| Final transition | Transition table rows 5-8 |

**This is the power of math-first design**: The code is obviously correct because it's a direct translation of proven mathematics.

---

## What We Didn't Write

Notice what's **absent** from this code:

- ❌ No `malloc` or `free`
- ❌ No string handling
- ❌ No file I/O
- ❌ No system calls (except in caller's time source)
- ❌ No floating point
- ❌ No recursion
- ❌ No global state
- ❌ No undefined behaviour

Each absence is intentional. Each absence makes the code easier to reason about.

---

## Exercises

### Exercise 4.1: Trace Execution

Trace through `hb_step` with these inputs:

**Scenario:**
- `now = 1000`, `hb_seen = 1`, `T = 500`, `W = 100`
- Previous state: `st = UNKNOWN`, `have_hb = 0`, `t_init = 0`

Walk through each line. What is the final state? Which transition table row applies?

### Exercise 4.2: Bug Hunt

This "simplified" version has bugs. Find them:

```c
void hb_step_buggy(hb_fsm_t *m, uint64_t now, uint8_t hb_seen, uint64_t T)
{
    if (hb_seen) m->last_hb = now;
    
    int64_t age = now - m->last_hb;  // Bug 1: signed type
    if (age > T) {
        m->st = STATE_DEAD;
    } else {
        m->st = STATE_ALIVE;        // Bug 2: no evidence check
    }
}
```

### Exercise 4.3: Alternative Design

Rewrite the final transition using a switch statement on `m->st`:

```c
switch (m->st) {
    case STATE_UNKNOWN:
        // ...
    case STATE_ALIVE:
        // ...
    case STATE_DEAD:
        // ...
}
```

Is it clearer? More error-prone? Does it change behaviour?

### Exercise 4.4: Contract Verification

For each of the three contracts, identify the specific lines of code that enforce it:

1. **CONTRACT-1 (Soundness)**: Which lines prevent false ALIVE?
2. **CONTRACT-2 (Liveness)**: Which lines ensure DEAD is eventually reported?
3. **CONTRACT-3 (Stability)**: Which lines prevent spurious transitions?

### Exercise 4.5: Edge Cases

What happens in these scenarios? Trace through the code:

1. First call to `hb_step` with `hb_seen = 0`
2. `now` is smaller than `last_hb` (clock jumped backward)
3. `hb_step` is called while `in_step = 1` (reentry)

---

## Key Takeaways

1. **The code is a transcription** — Every line maps to proven mathematics
2. **Simplicity is a feature** — 60 lines, no dependencies, no allocation
3. **Traceability matters** — You should be able to point to *why* each line exists
4. **Edge cases are handled** — Clock wrap, reentry, corruption all covered
5. **The math did the hard work** — The code just followed

---

## Next Lesson

In **Lesson 5: Testing & Hardening**, we'll:
- Write contract tests that verify our proofs
- Test boundary conditions (T-1, T, T+1)
- Inject faults and verify fail-safe behaviour
- Apply compiler hardening flags
- Run static analysis

The code looks simple. Now we prove it *is* simple.

---

> *"Simple things should be simple, complex things should be possible."* — Alan Kay

We've made the simple thing simple. The complex thing (proving correctness) was already done in Lesson 2.

---

[Previous: Lesson 3 — Structs](../03-structs/LESSON.md) | [Next: Lesson 5 — Testing](../05-testing/LESSON.md)
