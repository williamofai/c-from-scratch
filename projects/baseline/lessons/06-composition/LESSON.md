# c-from-scratch — Module 2: Baseline

## Lesson 6: Composition, Guarantees, and System Closure

---

## Why This Lesson Exists

Up to now, every lesson has done something unusual:

- We didn't optimise code
- We didn't tune heuristics
- We didn't eyeball graphs

Instead, we **constructed a system with guarantees**.

Lesson 6 answers three final questions:

1. What kind of systems have we been building?
2. Why does composition matter more than clever algorithms?
3. How does this scale beyond a single module?

**This lesson is not about new code. It's about thinking like a systems programmer who can prove things.**

---

## What We Built (Revisited)

Let's restate the modules in one sentence each:

### Module 1 — Pulse

**Proves that something exists in time.**

- Input: timestamps or events
- Output: "Is there a heartbeat?"
- Guarantees: No false heartbeat, deterministic, closed state machine

Pulse answers: *"Is anything happening?"*

### Module 2 — Baseline

**Proves whether values are normal.**

- Input: scalar observations
- Output: deviation signal + magnitude
- Guarantees: Convergence, Sensitivity, Stability, Spike Resistance

Baseline answers: *"Is what's happening normal?"*

### Together

```
Existence + Normality = Monitoring
```

Not alerts. Not dashboards. **Proof-carrying signals.**

---

## Composition: Why This Matters

Most systems fail not because components are bad, but because:

- They are stateful but undocumented
- They leak history
- They interact in undefined ways

Your modules do not.

**Why?**

Because both Pulse and Baseline are:

| Property | Meaning |
|----------|---------|
| Closed | State depends only on previous state + input |
| Total | Always returns a valid result |
| Bounded | O(1) memory, O(1) compute |
| Deterministic | Same inputs → same outputs |
| Contract-defined | Behaviour is specified, not implied |

**This makes composition safe.**

---

## The Pulse → Baseline Pipeline

Here is the key insight that ties the course together:

> Pulse outputs time deltas. Baseline consumes scalars.

That's it.

### Composition Pattern

```
event_t → Pulse → Δt → Baseline → deviation?
```

- Pulse answers: "Is the signal alive?"
- Baseline answers: "Is its rhythm normal?"

### What You Can Detect

| Failure Mode | Detected By |
|--------------|-------------|
| Missing heartbeats | Pulse |
| Bursts / stalls | Baseline on Δt |
| Clock drift | Baseline on Δt |
| Jitter increase | Baseline on variance |
| Gradual slowdown | Baseline convergence |

**No thresholds. No magic numbers. All statistical meaning comes from the signal itself.**

---

## Why This Is Better Than Typical Monitoring

Traditional monitoring stacks:

- Store unbounded history
- Compute statistics offline
- Alert via static rules
- Require constant tuning

Your approach:

- Stores state, not history
- Computes online
- Signals deviation magnitude
- Adapts automatically

This is the difference between:

> "We noticed something weird"

and

> "We can prove this is abnormal."

---

## The Big Idea: Statistics as State

The most important conceptual leap in this course:

> **Statistics are not calculations. They are state transitions.**

You didn't compute a mean. You **maintained** one.

You didn't store samples. You **encoded belief**.

This is why the system is:

- Memory-safe
- Predictable
- Testable
- Composable

And why every invariant test mattered.

---

## Why the Proof Harness Matters

Your test suite wasn't about coverage. It was about **truth**.

Each test answered a specific question:

| Question | Answered By |
|----------|-------------|
| Does it converge? | CONTRACT-1 |
| Will it detect change? | CONTRACT-2 |
| Will it cry wolf? | CONTRACT-3 |
| Can one bad value kill it? | CONTRACT-4 |
| Can it lie about state? | Invariants |
| Does chaos break it? | Fuzz tests |

**If a test fails, you don't guess. You know which guarantee broke.**

That's engineering, not debugging.

---

## What You Know Now (That Most Don't)

After this module, you can:

- Design closed state machines
- Encode statistical guarantees
- Prove properties with tests
- Avoid unbounded memory by construction
- Compose systems safely
- Explain **why** something works, not just **that** it works

**This is rare.**

Most engineers never learn to think this way.

---

## Where This Scales Next (Optional Directions)

You could now extend this approach to:

- Multivariate baselines (covariance as state)
- Percentile tracking (quantiles via state machines)
- Rate-of-change baselines
- Control systems
- Embedded anomaly detection
- Safety monitors
- Financial signal validation
- Network jitter detection

**Same pattern. Same discipline. Same guarantees.**

---

## The Complete Module Map

```
┌─────────────────────────────────────────────────────────────────┐
│                        MODULE 2: BASELINE                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Lesson 1: The Problem                                          │
│  ├── Why naive approaches fail                                  │
│  ├── The EMA insight                                            │
│  └── Four contracts to prove                                    │
│                                                                 │
│  Lesson 2: The Mathematical Model                               │
│  ├── Closed state: Sₜ = (μₜ, σₜ², nₜ, qₜ)                       │
│  ├── Update equations (EMA)                                     │
│  ├── FSM semantics                                              │
│  └── Contract definitions                                       │
│                                                                 │
│  Lesson 3: Structs & State Encoding                             │
│  ├── base_fsm_t structure                                       │
│  ├── Field-by-field justification                               │
│  ├── What is NOT stored                                         │
│  └── Invariants as design constraints                           │
│                                                                 │
│  Lesson 4: Code as Mathematical Transcription                   │
│  ├── Total functions                                            │
│  ├── Atomicity guards                                           │
│  ├── Fault handling                                             │
│  ├── EMA update (literal math)                                  │
│  └── FSM transitions                                            │
│                                                                 │
│  Lesson 5: Testing & Hardening                                  │
│  ├── Contract tests (4)                                         │
│  ├── Invariant tests (7)                                        │
│  ├── Fuzz tests                                                 │
│  └── Edge case tests                                            │
│                                                                 │
│  Lesson 6: Composition & Closure                                │
│  ├── Pulse + Baseline pipeline                                  │
│  ├── Statistics as state                                        │
│  └── Where this scales                                          │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│  DELIVERABLES                                                   │
│  ├── baseline.h      (API + contracts)                          │
│  ├── baseline.c      (implementation)                           │
│  ├── main.c          (demo)                                     │
│  └── test_baseline.c (proof harness)                            │
└─────────────────────────────────────────────────────────────────┘
```

---

## Final Takeaway

> **Good systems don't guess. They prove.**

Pulse proved existence.
Baseline proved normality.

Together, they demonstrate a way of building software where:

- State is minimal
- Behaviour is specified
- Failure modes are explicit
- Correctness is designed, not hoped for

**That's the real lesson.**

---

## End of Module 2

You have successfully:

- Treated statistics as state
- Made correctness provable
- Made testing a form of formal verification
- Preserved the systems-programming mindset throughout

This is not a tutorial baseline.

**This is a reference-quality statistical state machine.**

---

## What Comes Next

You now have:

- **Pulse** → existence in time
- **Baseline** → normality in value
- **Tests** → guarantees enforced

The next natural step is **Module 3**:

> Composition — where Pulse and Baseline become a single timing anomaly detector.

Or you can take these patterns and apply them to your own domain.

Either way, you now know how to build systems that prove their correctness.

**End of the Baseline Series.**
