# c-from-scratch — Module 2: Baseline

## Lesson 4: Code as Mathematical Transcription

> "Good code is not clever. Correct code is inevitable."

---

## Lesson Objective

By the end of this lesson, you will be able to:

- Read `baseline.c` and map every line to the math
- Explain why each guard, check, and branch exists
- Understand how contracts and invariants shape control flow
- Recognise the difference between defensive coding and provable coding

**This lesson is about how proofs become programs.**

---

## Where We Are in the Module

| Lesson | Purpose |
|--------|---------|
| Lesson 1 | Why anomaly detection is hard |
| Lesson 2 | Mathematical model (EMA + FSM) |
| Lesson 3 | Structs & state encoding |
| **Lesson 4** | **Code as proof transcription** |
| Lesson 5 | Testing & hardening (proof harness) |

Lesson 4 is the bridge between theory and verification.

---

## Core Idea

> The code is not an implementation choice. It is the **only possible implementation** that satisfies the contracts.

Every line exists because removing it would violate a guarantee.

---

## The Shape of baseline.c

At the highest level, `baseline.c` has exactly four responsibilities:

1. Reject invalid configurations
2. Detect and isolate faults
3. Update statistical state (EMA)
4. Apply FSM transitions

**Nothing else.**

There are:

- No dynamic allocations
- No buffers
- No hidden history
- No side channels

This keeps the system closed and auditable.

---

## 1. Total Functions: No Undefined Behaviour

### The Rule

**Every public function must be total.**

That means: for all valid inputs, it returns a valid result and leaves the system in a valid state.

In C, this is non-negotiable. Undefined behaviour is catastrophic.

### Example: base_step

```c
base_result_t base_step(base_fsm_t *b, double x)
```

This function is total:

- Always returns a valid `base_result_t`
- Never divides by zero
- Never reads uninitialised memory
- Never leaves the FSM inconsistent

Even when:

- x is NaN
- x is ±Inf
- the FSM is already faulted
- the function is re-entered illegally

**That is not defensive coding — that is contract enforcement.**

### Why This Matters

If any input can crash the system, then:

- CONTRACT-3 (stability) is meaningless
- CONTRACT-4 (spike resistance) is irrelevant
- The system cannot be trusted under stress

**Totality is the foundation of correctness.**

---

## 2. Atomicity and the Reentrancy Guard

### The Problem

C has no built-in protection against:

- signal handlers
- accidental recursion
- user misuse

So we enforce single-writer semantics manually.

### The Pattern

```c
if (b->in_step) {
    b->fault_reentry = 1;
    b->state = BASE_DEVIATION;
    return result;
}
b->in_step = 1;
```

This enforces **INV-4**:

```
(in_step == 0) when not executing base_step
```

### Design Choice

- Reentrancy is not ignored
- It is treated as a **semantic fault**
- The system enters DEVIATION
- The fault is sticky

This mirrors Pulse: *"If atomicity is violated, we cannot certify correctness."*

---

## 3. Faults Are Checked Before Math

### Principle

**Faults are semantic failures, not numerical ones.**

NaN and Inf do not mean "weird numbers". They mean:

> The system no longer knows what reality means.

### Code

```c
if (!is_finite(x)) {
    b->fault_fp = 1;
    b->state = BASE_DEVIATION;
    return result;
}
```

### Important Consequences

- LEARNING does not protect against faults
- Faulted input does not increment n
- Faults are sticky until `base_reset()`

This enforces:

- **INV-3** (fault → DEVIATION)
- **INV-7** (n increments only on valid input)

---

## 4. The EMA Update Is Literal Math

This is the heart of the system.

### Mathematical Definition (from Lesson 2)

```
deviation = xₜ - μₜ₋₁
μₜ = α·xₜ + (1 - α)·μₜ₋₁
σₜ² = α·deviation² + (1 - α)·σₜ₋₁²
σₜ = √σₜ²
z = |deviation| / σₜ
```

### C Transcription

```c
double mu_old = b->mu;
double deviation = x - mu_old;

double mu_new = alpha * x + (1.0 - alpha) * mu_old;
double var_new = alpha * (deviation * deviation)
               + (1.0 - alpha) * b->variance;
double sigma_new = sqrt(var_new);
```

### Why This Is So Strict

- No reordering
- No "optimisation"
- No combining steps

**Because changing the order breaks the proof.**

This is not an optimisation problem. It is a correctness problem.

---

## 5. The Variance Floor (No Division by Zero)

### The Reality Gap

- In math: σ = 0 is allowed
- In C: division by zero is undefined behaviour

So we introduce a **variance floor**.

### Code

```c
if (b->variance <= b->cfg.epsilon) {
    z = 0.0;
} else {
    z = abs_d(deviation) / sigma_new;
}
```

### Interpretation

When variance is too small to trust:

- The system refuses to quantify abnormality
- z is defined as 0
- The FSM remains stable

This preserves:

- Totality
- Stability
- Determinism

And directly supports **CONTRACT-3**.

---

## 6. FSM Transitions Are Separate from Math

### Why Separation Matters

Mixing statistics and state transitions makes reasoning impossible.

So the code structure is always:

1. Update statistics
2. **Then** update state

### Transition Logic

```c
switch (b->state) {
    case BASE_LEARNING:
        if (base_ready(b)) {
            b->state = BASE_STABLE;
        }
        break;

    case BASE_STABLE:
        if (z > b->cfg.k) {
            b->state = BASE_DEVIATION;
        }
        break;

    case BASE_DEVIATION:
        if (!base_faulted(b) && z <= b->cfg.k) {
            b->state = BASE_STABLE;
        }
        break;
}
```

Each branch maps exactly to the transition table from Lesson 2.

**No heuristics. No "clever" logic. No ambiguity.**

---

## 7. Why z Is Not Stored

### Design Rule

> If a value does not influence future state, it must not be stored.

- μ, σ², n, state → required for next step
- z → diagnostic only

So:

- z lives in `base_result_t`
- FSM state remains minimal and closed

This mirrors Pulse, which does not store derived timing metrics.

---

## 8. Reset Is a Mathematical Restart

`base_reset()` does not attempt recovery.

It performs a **full reinitialisation** of:

- statistics
- state
- faults

while preserving configuration.

This guarantees:

- No half-faulted states
- No hidden recovery logic
- All invariants restored

**Faults are sticky by design.**

---

## 9. Configuration Validation

```c
int base_init(base_fsm_t *b, const base_config_t *cfg)
```

This function enforces all configuration constraints:

```c
/* C1: 0 < alpha < 1 */
if (cfg->alpha <= 0.0 || cfg->alpha >= 1.0) {
    return -1;
}

/* C2: epsilon > 0 */
if (cfg->epsilon <= 0.0) {
    return -1;
}

/* C3: k > 0 */
if (cfg->k <= 0.0) {
    return -1;
}

/* C4: n_min >= ceil(2/alpha) */
uint32_t min_required = (uint32_t)ceil(2.0 / cfg->alpha);
if (cfg->n_min < min_required) {
    return -1;
}
```

**Invalid configurations are rejected at init, not discovered at runtime.**

---

## Student Exercises (Paper-Only)

### Exercise 1 — Mapping Proof to Code

For each of the following, identify the exact code line(s) that enforce it:

1. CONTRACT-4 (Spike Resistance)
2. INV-7 (Monotonic count)
3. "Faults do not increment n"

**Answer format:** invariant → code snippet → explanation.

### Exercise 2 — Removing a Line

Suppose you remove the variance floor:

```c
if (variance <= epsilon) z = 0;
```

Answer:

- Which contract is violated?
- What undefined behaviour can occur?
- Why would tests sometimes still pass?

### Exercise 3 — Reordering the Update

What breaks if you compute z **before** updating variance?

Explain in terms of:

- bias
- self-influence
- correctness of detection

---

## Key Takeaway

> This file is not "C code that implements statistics".
> It is a **mathematical proof that happens to compile**.

When you understand Lesson 4, you are no longer "coding".

**You are encoding guarantees.**

---

## Lesson 4 Checklist

By the end of Lesson 4, you should be able to:

- ☐ Trace every line of `baseline.c` to a contract or invariant
- ☐ Explain why faults are checked before math
- ☐ Explain why the update order is non-negotiable
- ☐ Identify what breaks if you remove the variance floor
- ☐ Understand totality as a correctness requirement

---

## Bridge to Lesson 5

Lesson 4 answered:

> "How do we write code that must be correct?"

Lesson 5 answers:

> "How do we prove that it actually is?"

That is where the proof harness begins.
