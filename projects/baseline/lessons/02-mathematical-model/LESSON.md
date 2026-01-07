# c-from-scratch — Module 2: Baseline

## Lesson 2: The Mathematical Model

---

## Purpose of This Lesson

In Lesson 1, we answered *why* naive anomaly detection fails and identified the properties we need.

In Lesson 2, we answer:

**What is the smallest mathematical system that satisfies all those properties?**

By the end of this lesson, you will:

- Understand the exact mathematical state of the Baseline monitor
- Derive the update rules from first principles
- See why the system is closed and O(1)
- Understand how z-scores emerge naturally
- Be ready to map math → structs in Lesson 3

**No code yet. Only math that earns its keep.**

This lesson is the specification. Lessons 3 and 4 are merely transcriptions.

---

## 1. From Observations to State

We observe a stream of scalar values:

```
x₁, x₂, x₃, ..., xₜ
```

Examples: CPU usage, latency, heartbeat interval (Δt from Pulse), queue depth.

A naive system stores history:

```
{x₁, x₂, ..., xₜ}
```

But this violates closure, boundedness, and determinism.

### The Core Insight

**We do not need history. We need state.**

We define a state vector Sₜ such that:

```
Sₜ = f(Sₜ₋₁, xₜ)
```

If this is true, the system is:

- Closed
- Bounded  
- Deterministic
- Implementable in C without allocation

---

## 2. What Must the State Contain?

We want to answer one question:

> "How abnormal is xₜ relative to what we've seen before?"

That requires three things:

1. A notion of **central tendency** (mean)
2. A notion of **spread** (variance)
3. A notion of **confidence** (enough observations)

### Minimal Statistical State

We define the minimal closed statistical state as:

```
Sₜ = (μₜ, σₜ², nₜ, qₜ)
```

Where:

| Symbol | Meaning |
|--------|---------|
| μₜ | Exponentially-weighted mean |
| σₜ² | Exponentially-weighted variance |
| nₜ | Number of observations seen |
| qₜ | FSM state ∈ {LEARNING, STABLE, DEVIATION} |

This is the smallest possible state that allows quantified anomaly detection.

**Anything less is insufficient. Anything more violates boundedness.**

---

## 3. Why Simple Moving Average Fails (Formally)

A Simple Moving Average (SMA) is defined as:

```
μₜ = (1/N) · Σₖ₌ₜ₋ₙ₊₁→ₜ xₖ
```

| Property | SMA | Why it Fails |
|----------|-----|--------------|
| Closure | ❌ | Depends on N past samples |
| Memory | ❌ | Requires buffer of size N |
| Determinism | ❌ | Behaviour depends on buffer fill |
| Spike Resistance | ❌ | One outlier corrupts N steps |
| C-friendly | ❌ | Requires shifting buffers |

**SMA is not just inefficient — it is not a valid state machine.**

---

## 4. Exponential Moving Average (EMA)

The Exponential Moving Average is defined as:

```
μₜ = α·xₜ + (1 − α)·μₜ₋₁
```

Where 0 < α < 1.

### Why EMA Is Special

EMA satisfies all required properties:

| Property | How EMA Delivers |
|----------|------------------|
| Closed | Depends only on μₜ₋₁ and xₜ |
| Bounded | O(1) memory |
| Adaptive | Recent data weighted more |
| Deterministic | Pure recurrence |
| Spike-resistant | Bounded influence |
| C-friendly | One multiply + add |

**This is not a heuristic. This is the only form that satisfies the contracts simultaneously.**

### Interpretation

EMA behaves like a sliding window of size ≈ 2/α, but without storing it.

This is not an approximation — it is a different model with better properties.

---

## 5. Variance as State (Not a Post-Process)

A mean alone is insufficient. We need to quantify how far an observation deviates.

We track variance using the same exponential structure:

```
deviationₜ = xₜ − μₜ₋₁
σₜ² = α·deviationₜ² + (1 − α)·σₜ₋₁²
```

**Important detail:** Deviation is computed using the **previous** mean.

This prevents the current observation from influencing its own anomaly score.

We then define:

```
σₜ = √(σₜ²)
```

---

## 6. The Z-Score Emerges Naturally

Once we have μₜ and σₜ, the normalised deviation is unavoidable:

```
zₜ = |xₜ − μₜ₋₁| / σₜ
```

This answers the question: **"How many sigmas away is this value?"**

### Interpretation

| z value | Meaning |
|---------|---------|
| z ≈ 0 | Completely normal |
| z ≈ 1 | Typical fluctuation |
| z ≈ 2 | Unusual but plausible |
| z ≥ 3 | Statistically rare |

This is not an arbitrary metric — it is the only dimensionless quantity that:

- Is scale-invariant
- Is comparable across systems
- Has known probabilistic meaning

---

## 7. Thresholding Without Guesswork

We introduce a single parameter k > 0.

**Decision rule:**

```
if zₜ > k → deviation
else      → normal
```

For normally distributed data:

| k | False positive probability |
|---|---------------------------|
| 2 | ≈ 4.5% |
| 3 | ≈ 0.3% |
| 4 | ≈ 0.006% |

**This is the entire detection logic.**

No magic numbers. No hand-tuned thresholds. Just geometry.

---

## 8. Learning vs Knowing

Early in the stream:

- Variance may be zero
- Mean may be meaningless
- z-score is undefined or unstable

So we introduce a **learning phase**.

Define:

```
READY ⇔ (n ≥ nₘᵢₙ) ∧ (σ² > ε)
```

Only when READY becomes true do we allow deviation detection.

This cleanly separates:

- Statistical readiness
- Anomaly detection

---

## 9. The Finite State Machine

We elevate statistics into a finite state machine.

### States

| State | Meaning |
|-------|---------|
| LEARNING | Statistics not yet reliable |
| STABLE | Baseline established |
| DEVIATION | Abnormal observation or fault |

### Transitions

```
LEARNING → STABLE      when n ≥ nₘᵢₙ and σ² > ε
STABLE → DEVIATION    when z > k
DEVIATION → STABLE    when z ≤ k and no fault
ANY → DEVIATION       on fault
```

**This is now a deterministic automaton, not "stats code".**

---

## 10. The Complete Update Sequence (Canonical)

Every observation executes exactly this sequence:

```
1. deviationₜ = xₜ − μₜ₋₁           (using mean BEFORE update)
2. μₜ        = α·xₜ + (1−α)·μₜ₋₁    (update mean)
3. σₜ²       = α·deviationₜ² + (1−α)·σₜ₋₁²  (update variance)
4. σₜ        = √σₜ²                  (update sigma)
5. zₜ        = |deviationₜ| / σₜ     (using sigma AFTER update)
```

**This ordering is not optional. Change it, and at least one contract breaks.**

---

## 11. Configuration Constraints (Enforced)

| Constraint | Meaning |
|------------|---------|
| 0 < α < 1 | Valid EMA smoothing factor |
| ε > 0 | Division safety (variance floor) |
| k > 0 | Meaningful deviation threshold |
| nₘᵢₙ ≥ ⌈2/α⌉ | EMA warm-up bound |

The last constraint is critical: it ties statistical confidence directly to the EMA's effective memory.

---

## 12. Proven Contracts (Theorems)

### CONTRACT-1 — Convergence

For stationary input with finite variance:

```
μₜ → E[X]  as t → ∞
```

**Why it holds:** EMA is a stable linear filter whose impulse response sums to 1.

### CONTRACT-2 — Sensitivity

A sustained deviation of size Δ produces:

```
μ shift ≈ (1 − (1 − α)ᵗ)·Δ
```

This reaches ~86% in 2/α steps. Detection is therefore O(1/α).

### CONTRACT-3 — Stability

For normally distributed input:

```
P(|Z| > k) ≈ 0.003   (k = 3)
```

False positives are bounded probabilistically, not heuristically.

### CONTRACT-4 — Spike Resistance

For a single outlier M:

```
μₜ − μₜ₋₁ = α·(M − μₜ₋₁)
```

So:

```
|Δμ| ≤ α·|M|
```

**This is a hard safety bound, not a probabilistic claim.**

No single input can corrupt the baseline. This is the safety guarantee static thresholds cannot provide.

---

## 13. Fault Model (Explicit)

Faults are **sticky** and include:

- NaN / ±Inf input
- Numerical overflow
- Reentrancy violation

**Fault consequences:**

- State forced to DEVIATION
- Observation count n does not increment
- Cleared only by `base_reset()`

This preserves honesty: a faulted system never claims normality.

---

## 14. Complexity Guarantees

| Property | Guaranteed |
|----------|------------|
| Memory | O(1) |
| Time per step | O(1) |
| Determinism | Yes |
| Closure | Yes |
| Recoverability | Yes |

---

## 15. What We Did Not Do

Deliberately absent:

- Buffers
- Windows
- Histograms
- Percentiles
- Machine learning
- Batch recomputation

**This is not a simplification. This is the minimal correct system.**

---

## 16. Composition with Pulse (Module 1)

**Pulse** proves existence in time:

```
∃ eventₜ
```

**Baseline** proves normality in value:

```
xₜ ~ N(μ, σ²)
```

**Composed system:**

1. Pulse emits inter-arrival times Δt
2. Baseline monitors Δt
3. Result: timing anomaly detection

This is **architectural composition**, not reuse-by-accident.

---

## Lesson 2 Checklist

By the end of Lesson 2, the student should be able to:

- ☐ Explain why EMA is required for closure
- ☐ Write the EMA recurrence from memory
- ☐ Explain why deviation uses μₜ₋₁
- ☐ Explain the difference between LEARNING and STABLE
- ☐ Understand z-score as a state-derived quantity
- ☐ State all four contracts

---

## Bridge to Lesson 3

In the next lesson, we will:

- Map each symbol (μ, σ², n, q) to a C struct field
- Encode the FSM transitions explicitly
- Enforce invariants at compile-time and runtime

**Math becomes memory layout. Equations become state transitions.**
