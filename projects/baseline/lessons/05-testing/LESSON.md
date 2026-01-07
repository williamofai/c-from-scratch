# c-from-scratch — Module 2: Baseline

## Lesson 5: Testing & Hardening

---

## Why This Lesson Exists

Most systems fail after deployment, not because the algorithm was wrong, but because:

- assumptions weren't enforced
- invariants silently broke
- edge cases were never exercised
- faults were handled inconsistently

This lesson closes the loop.

We move from "I think this is correct" to:

> **"I can prove this implementation satisfies its contracts."**

This is not unit testing. This is **verification-by-construction**.

---

## Philosophy: Tests as Proofs

Traditional unit tests ask:

> "Does this input produce this output?"

We ask:

> "Does this system **always** obey its guarantees?"

### Unit Tests vs Proof Harness

| Unit Test | Proof Harness |
|-----------|---------------|
| Example-based | Property-based |
| Tests behaviour | Tests contracts |
| Can pass accidentally | Fails if invariant violated |
| Output-focused | State-focused |

**A passing test here corresponds to a theorem holding.**

---

## What We Are Proving

From earlier lessons, the Baseline FSM claims four contracts:

```
CONTRACT-1: Convergence
CONTRACT-2: Sensitivity
CONTRACT-3: Stability
CONTRACT-4: Spike Resistance
```

And seven invariants:

```
INV-1: State domain closed
INV-2: Ready implies sufficient evidence
INV-3: Fault implies DEVIATION
INV-4: Atomicity (reentrancy guarded)
INV-5: Variance ≥ 0
INV-6: sigma == sqrt(variance)
INV-7: Observation count monotonic
```

Lesson 5 proves these claims hold in code.

---

## Test Categories

We divide tests by what they prove.

### 1. Contract Tests (Theorems)

Each contract gets one direct test.

| Test | Proves |
|------|--------|
| Convergence | EMA converges to true mean |
| Sensitivity | Large deviations are detected |
| Stability | Normal noise doesn't trigger alerts |
| Spike Resistance | One outlier can't corrupt baseline |

These tests validate the **mathematical guarantees**.

### 2. Invariant Tests (Structural Safety)

Invariants must hold after every step, regardless of input.

Examples:

- state never leaves {LEARNING, STABLE, DEVIATION}
- variance never goes negative
- faulted systems never claim stability
- counters never decrease

**If any invariant fails, the FSM is invalid.**

### 3. Fuzz Tests (Chaos Engineering)

We deliberately remove structure:

- random streams
- NaN / ±Inf injection
- long runs (100k+ steps)

The question becomes:

> "Can this state machine survive hostile input without lying?"

### 4. Edge Case Tests (Where Systems Die)

Edge cases are where naïve implementations fail:

- zero variance (division by zero)
- extreme values (overflow)
- invalid configuration
- reset semantics

**If these aren't tested, they will break in production.**

---

## Contract Tests Explained

### CONTRACT-1: Convergence

**Claim:** For stationary input, the EMA mean converges to the true mean.

**Test Idea:** Feed alternating values around 100 for a long time.

```c
for (int i = 0; i < 1000; i++) {
    double x = 100.0 + (i % 2 == 0 ? 0.5 : -0.5);
    base_step(&b, x);
}
assert(fabs(b.mu - 100.0) < 1.0);
```

**Why it matters:** If the baseline doesn't converge, everything else collapses.

### CONTRACT-2: Sensitivity

**Claim:** A deviation > kσ is detected in O(1/α) steps.

**Test Idea:** Establish a tight baseline, inject a large spike. Detection should be immediate.

```c
/* Establish baseline */
for (int i = 0; i < 100; i++) {
    base_step(&b, 100.0 + noise);
}

/* Inject spike */
base_result_t r = base_step(&b, 150.0);
assert(r.state == BASE_DEVIATION);
```

**Why it matters:** Late detection is as bad as no detection.

### CONTRACT-3: Stability

**Claim:** False positives are bounded by P(|Z| > k).

**Test Idea:** Feed noise strictly within learned variance. There should be zero deviations.

```c
int false_positives = 0;
for (int i = 0; i < 1000; i++) {
    double x = 100.0 + small_noise;
    base_result_t r = base_step(&b, x);
    if (r.state == BASE_DEVIATION) {
        false_positives++;
    }
}
assert(false_positives == 0);
```

**Why it matters:** A noisy detector is ignored.

### CONTRACT-4: Spike Resistance

**Claim:** A single outlier shifts the mean by at most α·M.

**Test Idea:** Inject a massive spike and measure Δμ.

```c
double mu_before = b.mu;
base_step(&b, 1000.0);  /* Massive spike */
double mu_after = b.mu;

double actual_shift = mu_after - mu_before;
double max_allowed = b.cfg.alpha * (1000.0 - mu_before);

assert(actual_shift <= max_allowed + 1e-9);
```

**Why it matters:** This is the safety contract. Break this, and one bad input corrupts the baseline forever.

---

## Invariant Tests Explained

### INV-3: Fault Implies Deviation

This invariant encodes **epistemic honesty**.

If the system has observed NaN, Inf, or reentrancy, it must admit uncertainty.

**Claiming STABLE while faulted is a lie.**

```c
base_step(&b, 0.0 / 0.0);  /* Inject NaN */

assert(base_faulted(&b));
assert(b.state == BASE_DEVIATION);
```

### INV-7: Monotonic Count

Observation count is a proxy for confidence.

If faulted inputs increment n, then confidence becomes meaningless.

```c
uint32_t n_before = b.n;
base_step(&b, 0.0 / 0.0);  /* Inject NaN */
assert(b.n == n_before);   /* n unchanged */
```

This invariant ensures:

- learning only happens on valid data
- statistics reflect reality

---

## Fuzz Testing: Trust Through Abuse

We intentionally try to break the system:

- random inputs
- random faults
- long runtimes

**We don't check outputs — we check invariants.**

```c
for (int i = 0; i < 100000; i++) {
    double x = random_double();
    base_result_t r = base_step(&b, x);
    
    /* All invariants must hold */
    assert(b.state == BASE_LEARNING ||
           b.state == BASE_STABLE ||
           b.state == BASE_DEVIATION);
    assert(b.variance >= 0.0);
    assert((r.is_deviation == 1) == (r.state == BASE_DEVIATION));
}
```

If invariants hold under chaos, the FSM is robust.

---

## Edge Case Tests

### Zero Variance

```c
/* Constant input = zero variance */
for (int i = 0; i < 100; i++) {
    base_result_t r = base_step(&b, 100.0);
    
    /* z should be 0 when variance <= epsilon */
    if (b.variance <= b.cfg.epsilon) {
        assert(r.z == 0.0);
    }
}
```

### Config Validation

```c
/* n_min too small for alpha */
base_config_t bad = { .alpha = 0.01, .n_min = 1, ... };
int result = base_init(&b, &bad);
assert(result == -1);  /* Must reject */
```

### Reset Clears Faults

```c
base_step(&b, 0.0 / 0.0);  /* Inject fault */
assert(base_faulted(&b));

base_reset(&b);
assert(!base_faulted(&b));
assert(b.state == BASE_LEARNING);
assert(b.n == 0);
```

---

## Hardening Checklist

Before shipping:

```
Build Quality:
☑ Compiles with -Wall -Wextra -Werror -pedantic
☑ No dynamic memory allocation
☑ All inputs validated
☑ NaN/Inf handled (fault_fp)
☑ Division by zero prevented (epsilon floor)
☑ Overflow checked (is_finite on computed values)
☑ Reentrancy detected (fault_reentry)
☑ Faults sticky until reset
☑ Reset restores clean state

Contract Tests:
☑ CONTRACT-1: Convergence
☑ CONTRACT-2: Sensitivity
☑ CONTRACT-3: Stability
☑ CONTRACT-4: Spike Resistance

Invariant Tests:
☑ INV-1: State domain
☑ INV-2: Ready implies not learning
☑ INV-3: Fault implies deviation
☑ INV-5: Variance non-negative
☑ INV-7: Monotonic count

Fuzz Tests:
☑ 100K random observations
☑ NaN injection
☑ Inf injection
☑ Edge cases
```

**This is what "production-ready C" looks like.**

---

## Test Output

```
╔════════════════════════════════════════════════════════════════╗
║              Baseline Contract Test Suite                      ║
╚════════════════════════════════════════════════════════════════╝

Contract Tests:
  [PASS] CONTRACT-1: Convergence (error=0.0263)
  [PASS] CONTRACT-2: Sensitivity (z=3.16, detected immediately)
  [PASS] CONTRACT-3: Stability (0 false positives in 1000)
  [PASS] CONTRACT-4: Spike Resistance (Spike=1000, Shift=90.00, Max=90.00)

Invariant Tests:
  [PASS] INV-1: State always in valid domain
  [PASS] INV-2: Ready implies sufficient evidence
  [PASS] INV-3: Fault implies DEVIATION
  [PASS] INV-5: Variance always non-negative
  [PASS] INV-7: Count monotonically increasing
  [PASS] INV-7: Faulted input does not increment n

Fuzz Tests:
  [PASS] Fuzz: 100000 random observations, invariants held
  [PASS] Fuzz: NaN injection handled safely
  [PASS] Fuzz: +Inf handled (fault_fp set)
  [PASS] Fuzz: -Inf handled (fault_fp set)

Edge Case Tests:
  [PASS] Edge: Zero variance handled
  [PASS] Edge: Extreme finite values handled
  [PASS] Edge: Config validation rejects invalid params
  [PASS] Edge: Reset clears faults and state

══════════════════════════════════════════════════════════════════
  Results: 18/18 tests passed
══════════════════════════════════════════════════════════════════
```

---

## Key Insight

> **Tests don't prove correctness. Contracts define correctness. Tests verify contracts.**

When all tests pass, you haven't shown:

> "It works on my machine."

You've shown:

> "This implementation satisfies its specification."

**That's the difference between code and engineering.**

---

## Lesson 5 Checklist

By the end of Lesson 5, you should be able to:

- ☐ Write a test that proves a specific contract
- ☐ Explain why fuzz testing checks invariants, not outputs
- ☐ Identify which contract is violated when a test fails
- ☐ Understand the difference between unit tests and proof harnesses
- ☐ Apply the hardening checklist to your own code

---

## Bridge to Lesson 6

Lesson 5 answered:

> "How do we prove the code is correct?"

Lesson 6 answers:

> "What have we actually built, and where does it go from here?"

That is where composition and system closure come together.
