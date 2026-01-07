# c-from-scratch — Module 2: Baseline

## Lesson 3: Structs & State Encoding

---

## Why This Lesson Exists

So far, we've done two things:

- **Lesson 1:** Identified the problem and the contracts we must satisfy
- **Lesson 2:** Defined the mathematical system that satisfies those contracts

Now comes the hardest step for most programmers:

**Turning math into memory without losing meaning.**

Most bugs in systems programming are not algorithmic. They are state bugs.

This lesson teaches you how to encode a mathematical state machine into a C struct without breaking the guarantees you just proved.

---

## Core Principle

> **A struct is not a bag of variables. A struct is a state vector.**

Every field exists because the math requires it. Nothing more. Nothing less.

If a field:

- doesn't correspond to a mathematical symbol
- isn't required for a contract
- or can be derived from other fields

…it does not belong in the struct.

---

## The Mathematical State (Recap)

From Lesson 2, the baseline system is defined by:

```
Sₜ = (μₜ, σₜ², nₜ, qₜ)
```

| Symbol | Meaning |
|--------|---------|
| μₜ | Exponentially-weighted mean |
| σₜ² | Exponentially-weighted variance |
| nₜ | Observation count |
| qₜ | FSM state (LEARNING / STABLE / DEVIATION) |

This is a closed system:

```
Sₜ₊₁ = f(Sₜ, xₜ₊₁)
```

Your struct must represent **exactly** this state.

---

## Encoding the State Vector in C

Here is the final state encoding:

```c
typedef struct {
    /* Configuration (immutable after init) */
    base_config_t cfg;

    /* Mathematical state */
    double       mu;        /* μₜ */
    double       variance;  /* σₜ² */
    double       sigma;     /* √σₜ² (cached) */
    uint32_t     n;         /* nₜ */
    base_state_t state;     /* qₜ */

    /* Fault flags (C reality) */
    uint8_t      fault_fp;
    uint8_t      fault_reentry;

    /* Atomicity guard */
    uint8_t      in_step;
} base_fsm_t;
```

**This is not accidental. Every field exists for a reason.**

---

## Field-by-Field Justification

### mu — The Mean

```c
double mu;
```

- Represents μₜ
- Updated every step
- Required for:
  - deviation calculation
  - convergence (CONTRACT-1)
  - spike resistance (CONTRACT-4)

**If you lose mu, the system has no memory.**

### variance — The Variance

```c
double variance;
```

- Represents σₜ²
- Stored, not derived
- Required for:
  - z-score computation
  - stability guarantees
  - avoiding unbounded history

**Never recompute variance from history. That would violate closure.**

### sigma — Cached √Variance

```c
double sigma;
```

Why store this?

- √ is expensive
- σ is used every step
- Keeping it cached enforces INV-6: `sigma == √variance`

This is a performance optimisation that **preserves correctness**, not a shortcut.

### n — Observation Count

```c
uint32_t n;
```

This is not just a counter. It represents **statistical confidence**.

Used for:

- LEARNING → STABLE transition
- enforcing minimum evidence (n_min)
- invariants about readiness

**Important rule:** Faulted inputs must NOT increment n. Otherwise, confidence lies.

### state — The FSM State

```c
base_state_t state;
```

This is not derived from math — it is a **control state**.

Why it must be explicit:

- Learning vs detection must be separated
- Fault handling must override normal logic
- Transitions must be deterministic

**FSM state is how math becomes behaviour.**

---

## Configuration Is State (But Immutable)

```c
base_config_t cfg;
```

Why store config inside the FSM?

Because α, ε, k, nₘᵢₙ are **properties of the machine**, not the environment.

Embedding them:

- makes the FSM self-contained
- prevents mismatched parameters
- ensures determinism

**Once initialised, cfg never changes.**

---

## Fault Flags: Reality Leaks In

Mathematics assumes perfect numbers. C does not.

```c
uint8_t fault_fp;
uint8_t fault_reentry;
```

These fields exist to handle **physical reality**:

| Fault | Meaning |
|-------|---------|
| fault_fp | NaN / Inf / numeric corruption |
| fault_reentry | Atomicity violation |

**Key design choice:** Faults are sticky.

Once a system lies, it cannot claim normality again until explicitly reset.

This mirrors Pulse's DEAD state.

---

## in_step — Enforcing Atomicity

```c
uint8_t in_step;
```

This single byte enforces INV-4: **No reentrant execution**.

Why this matters:

- Prevents partial updates
- Avoids torn state
- Makes the FSM total and deterministic

If `base_step()` is re-entered:

```
→ fault_reentry = 1
→ state = DEVIATION
→ system refuses to lie
```

---

## What Is Not Stored (By Design)

### ❌ z-score

```c
double z;  // NOT STORED
```

Why?

- z is derived
- Not needed for future steps
- Storing it breaks minimality

Same design choice as Pulse: **derived values live in results, not state.**

### ❌ History Buffer

```c
double history[...];  // NEVER
```

Why?

- Violates closure
- Violates bounded memory
- Breaks determinism

**History is a crutch, not a solution.**

---

## Zero-Initialisation as a Safety Property

This is intentional:

```c
base_fsm_t b = {0};
```

Yields:

- state = BASE_LEARNING
- mu = 0
- variance = 0
- n = 0

This mirrors Pulse's STATE_UNKNOWN.

**Zero-initialisation must always be safe.**

If your struct cannot survive being zeroed, it is not robust.

---

## Invariants as Design Constraints

Your struct enforces invariants simply by existing:

| Invariant | Enforced By |
|-----------|-------------|
| INV-1: State domain | enum |
| INV-2: Ready implies evidence | n + variance |
| INV-3: Fault implies DEVIATION | fault flags |
| INV-5: variance ≥ 0 | EMA update |
| INV-6: sigma == √variance | cached update |
| INV-7: monotonic n | controlled increment |

**Good structs prevent invalid states.**

---

## The Configuration Structure

```c
typedef struct {
    double   alpha;      /* EMA smoothing factor ∈ (0, 1)              */
    double   epsilon;    /* Variance floor for safe z-score computation */
    double   k;          /* Deviation threshold (z-score units)        */
    uint32_t n_min;      /* Minimum observations before STABLE         */
} base_config_t;
```

### Constraints (Enforced in base_init)

```
C1: 0 < alpha < 1
C2: epsilon > 0
C3: k > 0
C4: n_min >= ceil(2/alpha)
```

### Default Values

```c
static const base_config_t BASE_DEFAULT_CONFIG = {
    .alpha   = 0.1,      /* Effective window ≈ 20 observations */
    .epsilon = 1e-9,     /* Variance floor for numerical safety */
    .k       = 3.0,      /* Three-sigma deviation threshold     */
    .n_min   = 20        /* ceil(2/alpha) = 20 (EMA warm-up)    */
};
```

---

## The Result Structure

```c
typedef struct {
    double       z;             /* Computed z-score: |deviation| / σₜ   */
    double       deviation;     /* Raw deviation: (xₜ - μₜ₋₁)           */
    base_state_t state;         /* FSM state after this observation     */
    uint8_t      is_deviation;  /* Convenience: state == BASE_DEVIATION */
} base_result_t;
```

**Note:** These are **derived values**. They exist in results, not in state.

---

## Mental Model

Think of `base_fsm_t` as:

> A serialised mathematical proof.

Each field:

- exists because a theorem requires it
- is updated according to a rule
- participates in a contract

You are not "storing variables". You are **encoding a state machine that proves normality**.

---

## Lesson 3 Checklist

By the end of Lesson 3, you should be able to:

- ☐ Explain why every field exists
- ☐ Justify why certain values are not stored
- ☐ Design structs that encode mathematical state
- ☐ Spot "junk fields" in real code
- ☐ Reason about invariants from memory layout alone

---

## Bridge to Lesson 4

In the next lesson, we will:

- Translate the mathematical update rules into C
- Enforce contracts through control flow
- See how totality and determinism shape every line

**Structs become code. State transitions become functions.**
