# Lesson 3: Structs & Data Dictionary

## The Shape of the Solution

> "A data structure is a contract with the future. Every field is a promise you must keep."

In Lesson 2, we proved our state machine is **closed** and **total**—every possible input leads to exactly one defined output. Now we must translate that mathematical certainty into C structures that **preserve those guarantees**.

This is where most C tutorials fail. They show you *what* a struct is, but not *why* each field exists, *what* constraints it must satisfy, and *how* violations are detected.

---

## The Core Principle: Every Field Has a Purpose

Before writing a single line of code, we establish three rules:

1. **PURPOSE**: Why does this field exist? What contract does it serve?
2. **CONSTRAINT**: What values are valid? What makes a value invalid?
3. **INVARIANT**: What must always be true about this field in relation to others?

If you can't answer all three questions, you don't understand your data yet.

---

## Data Dictionary

### 1. State Enumeration: `state_t`

```c
typedef enum {
    STATE_UNKNOWN = 0,
    STATE_ALIVE   = 1,
    STATE_DEAD    = 2
} state_t;
```

#### Why an enum?

**The alternative (and why it's wrong):**
```c
#define STATE_UNKNOWN 0
#define STATE_ALIVE   1
#define STATE_DEAD    2
int state;  // Could be ANY integer!
```

This allows `state = 47`, which is meaningless. The type system provides no protection.

**Why enum is better:**
- **Self-documenting**: The compiler knows these are the only valid values
- **Debugger-friendly**: Shows `STATE_ALIVE` instead of `1`
- **Switch completeness**: Compiler can warn if you miss a case

#### Why these specific values?

| Value | Meaning | Why This Number? |
|-------|---------|------------------|
| `0` | UNKNOWN | Zero-initialisation gives safe default. `memset(struct, 0, ...)` produces UNKNOWN, not ALIVE. |
| `1` | ALIVE | Positive = good. Non-zero = active/true in C idioms. |
| `2` | DEAD | Distinct from both. Order doesn't imply severity. |

#### The Mathematical Connection

From Lesson 2, our state space was:

```
S = { UNKNOWN, ALIVE, DEAD }
```

This enum is a **direct transcription**. No more, no less. The math said three states; the code has three states. This is not coincidence—it's discipline.

#### Why not add more states?

You might think: "What about `STATE_STARTING` or `STATE_SHUTTING_DOWN`?"

**Answer**: Our contracts don't require them. Adding states without mathematical justification creates:
- More transitions to prove correct
- More code paths to test
- More ways to have bugs

The math is minimal. The code must be minimal.

---

### 2. Time Representation: `uint64_t`

```c
uint64_t t_init;    // Boot/reset reference time
uint64_t last_hb;   // Timestamp of most recent heartbeat
```

#### Why `uint64_t` and not `time_t`?

| Type | Problems |
|------|----------|
| `time_t` | Size varies (32-bit on some systems). Signedness varies. May overflow in 2038. |
| `int` | Only 32 bits. Signed overflow is undefined behaviour in C. |
| `unsigned int` | Only 32 bits. Wraps at ~4.3 billion. |
| `uint64_t` | Exactly 64 bits. Always unsigned. Wraps predictably at 2^64. |

#### The Modular Arithmetic Connection

From Lesson 2, we defined time as:

```
t ∈ ℤ₂^N  where N = 64
```

This means time is a **modular counter**—it wraps around at 2^64. Using `uint64_t` gives us:
- **Exact bit width**: We know N = 64, always
- **Defined overflow**: Unsigned overflow wraps (guaranteed by C standard)
- **No undefined behaviour**: Critical for safety-critical code

#### Why Two Time Fields?

| Field | Purpose | When Set |
|-------|---------|----------|
| `t_init` | Reference point for "how long since boot" | Once, at initialisation |
| `last_hb` | Reference point for "how long since heartbeat" | Each time a heartbeat arrives |

These serve different contracts:
- **CONTRACT-2 (Liveness)** needs `last_hb` to detect timeout
- **Initialisation window** needs `t_init` to allow warm-up period

#### The Age Computation

To find "how old" a timestamp is:

```c
uint64_t age = now - then;  // Modular subtraction
```

**Why this works (the half-range rule):**

If `now = 10` and `then = 5`, then `age = 5`. Simple.

But what if the clock wrapped? If `now = 3` and `then = 0xFFFFFFFFFFFFFFFF` (max value), then:
```
age = 3 - 0xFFFFFFFFFFFFFFFF = 4  (due to modular arithmetic)
```

This is still correct! The wrap happened between `then` and `now`.

**When it breaks**: If the age exceeds half the range (2^63), we can't tell the difference between "very old" and "wrapped backwards". That's why we validate:

```c
static inline uint8_t age_valid(uint64_t age) {
    return (age < (1ULL << 63));  // Must be less than half-range
}
```

---

### 3. Evidence Tracking: `have_hb`

```c
uint8_t have_hb;  // Evidence flag: ≥1 heartbeat observed
```

#### Why a Separate Flag?

**The tempting (wrong) approach:**
```c
// Use last_hb == 0 to mean "no heartbeat yet"
if (last_hb == 0) { /* no evidence */ }
```

**Why this fails:**
- What if the first heartbeat arrives at time 0?
- What if the clock wraps back to 0?
- You've overloaded a timestamp with a boolean meaning

**The principle**: Each piece of information gets its own field. Don't overload semantics.

#### Why `uint8_t` Instead of `bool`?

| Type | Issues |
|------|--------|
| `bool` | Requires `<stdbool.h>`. Size varies. Some compilers don't support it. |
| `int` | Wastes space. No semantic indication this is boolean. |
| `uint8_t` | Exactly 1 byte. Universal support. Clear intent. |

#### The Mathematical Connection

From Lesson 2, our transition table had conditions like:
- "UNKNOWN + no heartbeat + init window not expired → UNKNOWN"
- "UNKNOWN + heartbeat seen → ALIVE"

The `have_hb` flag directly encodes the distinction between "we have evidence" and "we have no evidence". Without it, we couldn't distinguish UNKNOWN (never seen heartbeat) from DEAD (heartbeat expired).

---

### 4. Fault Detection: `fault_time` and `fault_reentry`

```c
uint8_t fault_time;     // Clock corruption detected
uint8_t fault_reentry;  // Atomicity violation detected
```

#### Why Track Faults?

Our contracts are **conditional**:
- CONTRACT-1 (Soundness): "Never report ALIVE incorrectly **if time is valid**"
- CONTRACT-2 (Liveness): "Eventually report DEAD **if polling continues**"

When conditions are violated, we need to know. Silently continuing is worse than a visible fault.

#### `fault_time`: Clock Corruption

**What triggers it:**
```c
if (!age_valid(a_init) || !age_valid(a_hb)) {
    m->fault_time = 1;
    m->st = STATE_DEAD;  // Fail-safe
}
```

**Why this matters:**
- Clocks can jump backwards (NTP corrections, VM snapshots, hardware faults)
- Clocks can jump forwards (system resume from suspend)
- Either can produce invalid ages (> 2^63)

**The design choice**: Instead of trying to "handle" corrupt time (how?), we:
1. Detect it
2. Record it (for diagnostics)
3. Fail safe (report DEAD—never falsely ALIVE)

**Why DEAD is the safe choice:**
- If we report ALIVE incorrectly, a dead process continues unnoticed (system failure)
- If we report DEAD incorrectly, a healthy process gets restarted (minor inconvenience)

Asymmetric consequences demand asymmetric caution.

#### `fault_reentry`: Atomicity Violation

**What triggers it:**
```c
if (m->in_step) {
    m->fault_reentry = 1;
    m->st = STATE_DEAD;
    return;
}
m->in_step = 1;
// ... do work ...
m->in_step = 0;
```

**Why this matters:**

In embedded systems or signal handlers, `hb_step()` might be called while it's already executing. This could leave the struct in an inconsistent state:
- `last_hb` updated but `st` not yet computed
- Half-written values

**The design choice**:
- Detect reentry via a guard flag
- Abort immediately
- Record the violation
- Fail safe (DEAD)

**Why not use mutexes?**
- Not available in all environments (bare metal, some RTOSes)
- Add complexity and potential deadlocks
- Our solution: detect and fail, don't prevent

---

### 5. Reentrancy Guard: `in_step`

```c
uint8_t in_step;  // Non-zero while hb_step is executing
```

#### The Pattern

```c
void hb_step(...) {
    if (m->in_step) { /* FAULT! */ }
    m->in_step = 1;
    
    // ... actual work ...
    
    m->in_step = 0;
}
```

#### Why This Isn't Thread-Safe (And That's OK)

**What this guard does:**
- Detects if `hb_step` is called recursively (e.g., from a signal handler)
- Detects if `hb_step` is interrupted mid-execution

**What this guard doesn't do:**
- Protect against concurrent calls from different threads
- Provide mutual exclusion

**Why that's acceptable:**
- Our contract (Lesson 2, Section 8) says "Single-writer or critical section enforced"
- Thread safety is the **caller's** responsibility
- The guard catches **accidental** reentry, not **intentional** concurrency

**The principle**: Know what guarantees you're providing and which you're requiring.

---

## The Complete Structure

```c
typedef struct {
    state_t  st;           // Current state ∈ { UNKNOWN, ALIVE, DEAD }
    uint64_t t_init;       // Boot/reset reference time
    uint64_t last_hb;      // Timestamp of most recent heartbeat
    uint8_t  have_hb;      // Evidence flag: ≥1 heartbeat observed
    uint8_t  fault_time;   // Clock corruption detected
    uint8_t  fault_reentry;// Atomicity violation detected
    uint8_t  in_step;      // Reentrancy guard (non-zero = executing)
} hb_fsm_t;
```

### Memory Layout Analysis

| Field | Size | Offset | Notes |
|-------|------|--------|-------|
| `st` | 4 bytes | 0 | Enum is typically int-sized |
| (padding) | 4 bytes | 4 | Alignment for uint64_t |
| `t_init` | 8 bytes | 8 | 8-byte aligned |
| `last_hb` | 8 bytes | 16 | 8-byte aligned |
| `have_hb` | 1 byte | 24 | |
| `fault_time` | 1 byte | 25 | |
| `fault_reentry` | 1 byte | 26 | |
| `in_step` | 1 byte | 27 | |
| (padding) | 4 bytes | 28 | Struct alignment to 8 |
| **Total** | 32 bytes | | |

**Why we don't pack the struct:**
- Unaligned access is slow (or crashes on some architectures)
- 32 bytes is small enough
- Correctness over cleverness

---

## Struct Invariants

These must be true at **all times** (except transiently inside `hb_step`):

### INV-1: State Validity
```
st ∈ { STATE_UNKNOWN, STATE_ALIVE, STATE_DEAD }
```
No other values are possible.

### INV-2: Evidence Consistency
```
(st == STATE_ALIVE) → (have_hb == 1)
```
Can't be alive without evidence.

### INV-3: Fault → Dead
```
(fault_time == 1 ∨ fault_reentry == 1) → (st == STATE_DEAD)
```
Any fault forces DEAD state.

### INV-4: Reentrancy Clean
```
(in_step == 0) outside of hb_step execution
```
Guard is only set during active computation.

---

## The Complete Header File

Here is `pulse.h`—the complete interface with contracts documented:

```c
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
 */

#ifndef PULSE_H
#define PULSE_H

#include <stdint.h>

/**
 * Visible states of the liveness monitor.
 * 
 * STATE_UNKNOWN: No evidence yet (initial state)
 * STATE_ALIVE:   Recent heartbeat observed
 * STATE_DEAD:    Heartbeat timeout expired or fault detected
 * 
 * Zero-initialisation yields STATE_UNKNOWN (safe default).
 */
typedef enum {
    STATE_UNKNOWN = 0,
    STATE_ALIVE   = 1,
    STATE_DEAD    = 2
} state_t;

/**
 * Heartbeat Finite State Machine structure.
 * 
 * All fields are explicitly owned—no external references.
 * No dynamic allocation. Fixed size. Fully deterministic.
 * 
 * INVARIANTS:
 *   INV-1: st ∈ { UNKNOWN, ALIVE, DEAD }
 *   INV-2: (st == ALIVE) → (have_hb == 1)
 *   INV-3: (fault_time ∨ fault_reentry) → (st == DEAD)
 *   INV-4: (in_step == 0) when not executing hb_step
 */
typedef struct {
    state_t  st;            /* Current state ∈ S                        */
    uint64_t t_init;        /* Boot/reset reference time                */
    uint64_t last_hb;       /* Timestamp of most recent heartbeat       */
    uint8_t  have_hb;       /* Evidence flag: ≥1 heartbeat observed     */
    uint8_t  fault_time;    /* Fault: clock corruption detected         */
    uint8_t  fault_reentry; /* Fault: atomicity violation detected      */
    uint8_t  in_step;       /* Reentrancy guard (1 = executing)         */
} hb_fsm_t;

/**
 * Initialise the state machine.
 * 
 * PRECONDITIONS:
 *   - m is a valid pointer to uninitialised hb_fsm_t
 *   - now is current time from monotonic source
 * 
 * POSTCONDITIONS:
 *   - m->st == STATE_UNKNOWN
 *   - m->t_init == now
 *   - All fault flags cleared
 *   - m->have_hb == 0
 * 
 * @param m   Pointer to state machine structure
 * @param now Current timestamp
 */
void hb_init(hb_fsm_t *m, uint64_t now);

/**
 * Execute one step of the state machine.
 * 
 * PRECONDITIONS:
 *   - m was initialised via hb_init
 *   - now is current time (monotonic, may wrap)
 *   - T > 0 (timeout period)
 *   - W >= 0 (initialisation window, may be 0)
 * 
 * POSTCONDITIONS:
 *   - m->st updated according to transition function δ
 *   - If hb_seen, m->last_hb == now and m->have_hb == 1
 *   - Faults detected → m->st == STATE_DEAD
 * 
 * CONTRACTS PRESERVED:
 *   - CONTRACT-1 (Soundness): Will not set ALIVE unless evidence valid
 *   - CONTRACT-2 (Liveness): Will set DEAD if age > T
 *   - CONTRACT-3 (Stability): No transition without cause
 * 
 * @param m       Pointer to state machine structure
 * @param now     Current timestamp  
 * @param hb_seen 1 if heartbeat observed this step, 0 otherwise
 * @param T       Timeout threshold (in time units)
 * @param W       Initialisation window (in time units)
 */
void hb_step(hb_fsm_t *m, uint64_t now, uint8_t hb_seen, 
             uint64_t T, uint64_t W);

/**
 * Query current state.
 * 
 * @param m Pointer to state machine structure
 * @return  Current state (UNKNOWN, ALIVE, or DEAD)
 */
static inline state_t hb_state(const hb_fsm_t *m) {
    return m->st;
}

/**
 * Check if any fault has been detected.
 * 
 * @param m Pointer to state machine structure
 * @return  1 if fault detected, 0 otherwise
 */
static inline uint8_t hb_faulted(const hb_fsm_t *m) {
    return m->fault_time || m->fault_reentry;
}

#endif /* PULSE_H */
```

---

## Exercises

### Exercise 3.1: Field Justification
For each field in `hb_fsm_t`, write one sentence explaining what contract it serves. Reference the contract number from Lesson 2.

### Exercise 3.2: Invariant Violation
Write a hypothetical sequence of operations that would violate INV-2. Explain why our implementation prevents this.

### Exercise 3.3: Alternative Designs
Consider replacing `have_hb` with a sentinel value in `last_hb` (e.g., `UINT64_MAX` means "no heartbeat"). What problems would this create? When might it still be acceptable?

### Exercise 3.4: Memory Layout
On a 32-bit system, how might the struct layout differ? Would the code still work correctly? Why or why not?

### Exercise 3.5: Extension
If we wanted to add "time since last heartbeat" as a queryable value, what would we need to add to the struct? What new invariants would be required?

---

## Key Takeaways

1. **Every field has a purpose** traced back to a mathematical requirement
2. **Constraints are explicit**, not implicit or assumed  
3. **Invariants are documented** and mechanically checkable
4. **Types are chosen deliberately** (`uint64_t` for defined overflow, `uint8_t` for portability)
5. **Faults are detected and recorded**, not ignored
6. **The struct is the contract** between the math and the code

---

## Next Lesson

In **Lesson 4: Code**, we'll implement `hb_init` and `hb_step`. You'll see that with the struct properly designed and the math proven, the code practically writes itself—each line is a direct transcription of a mathematical statement.

*"The code is just the math, wearing a different costume."*
