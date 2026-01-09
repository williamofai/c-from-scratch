# Lesson 2: Mathematical Model

## Composing State Machines

> "The composition inherits the contracts. Prove the components, compose them, trust the result."

This lesson defines the mathematical model for our composed timing health monitor. Every decision here will directly inform the C structures and code in later lessons.

---

## 1. State Definition

The composed system has state from both modules plus a mapping:

```
S_timing = (S_pulse, S_baseline, q_timing)
```

Where:
- `S_pulse` = Pulse FSM state (tracking heartbeat existence)
- `S_baseline` = Baseline FSM state (tracking Δt normality)
- `q_timing` = Composed state ∈ {INITIALIZING, HEALTHY, UNHEALTHY, DEAD}

### Component States (Review)

**Pulse States:**
```
S_pulse ∈ { UNKNOWN, ALIVE, DEAD }
```

**Baseline States:**
```
S_baseline ∈ { LEARNING, STABLE, DEVIATION }
```

### Composed States

```
q_timing ∈ { INITIALIZING, HEALTHY, UNHEALTHY, DEAD }
```

| State | Meaning |
|-------|---------|
| INITIALIZING | Neither module has sufficient evidence |
| HEALTHY | Pulse is ALIVE and Baseline is STABLE |
| UNHEALTHY | Pulse is ALIVE but Baseline shows DEVIATION |
| DEAD | Pulse is DEAD (no heartbeats) |

---

## 2. State Mapping

The composed state is a **pure function** of component states:

```
q_timing = map(S_pulse, S_baseline)
```

### Complete Mapping Table

| Pulse State | Baseline State | Timing State | Justification |
|-------------|----------------|--------------|---------------|
| DEAD | LEARNING | DEAD | Dead overrides all |
| DEAD | STABLE | DEAD | Dead overrides all |
| DEAD | DEVIATION | DEAD | Dead overrides all |
| UNKNOWN | LEARNING | INITIALIZING | No evidence yet |
| UNKNOWN | STABLE | INITIALIZING | Pulse not confirmed |
| UNKNOWN | DEVIATION | INITIALIZING | Pulse not confirmed |
| ALIVE | LEARNING | INITIALIZING | Stats not ready |
| ALIVE | STABLE | **HEALTHY** | Full evidence, normal |
| ALIVE | DEVIATION | **UNHEALTHY** | Alive but anomalous |

### Key Observations

1. **DEAD is absorbing:** If pulse is dead, nothing else matters
2. **UNKNOWN blocks health:** Can't claim healthy without existence evidence
3. **LEARNING blocks health:** Can't claim healthy without statistical evidence
4. **Only one path to HEALTHY:** ALIVE × STABLE

---

## 3. Transition Diagram

```
                    ┌─────────────────────┐
                    │    INITIALIZING     │
                    │   (learning phase)  │
                    └──────────┬──────────┘
                               │
                pulse=ALIVE ∧ baseline=STABLE
                               │
                               ▼
                    ┌─────────────────────┐
        ┌──────────│       HEALTHY       │──────────┐
        │          │  (normal rhythm)    │          │
        │          └─────────────────────┘          │
        │                    │                      │
  baseline=DEVIATION    pulse=DEAD           baseline=STABLE
        │                    │                      │
        ▼                    ▼                      │
┌───────────────────┐ ┌─────────────────────┐      │
│     UNHEALTHY     │ │        DEAD         │      │
│ (timing anomaly)  │ │  (no heartbeat)     │      │
└─────────┬─────────┘ └─────────────────────┘      │
          │                                         │
          └─────────────────────────────────────────┘
```

### Transition Rules

| From | To | Condition |
|------|-----|-----------|
| INITIALIZING | HEALTHY | pulse=ALIVE ∧ baseline=STABLE |
| INITIALIZING | UNHEALTHY | pulse=ALIVE ∧ baseline=DEVIATION |
| INITIALIZING | DEAD | pulse=DEAD |
| HEALTHY | UNHEALTHY | baseline=DEVIATION |
| HEALTHY | DEAD | pulse=DEAD |
| UNHEALTHY | HEALTHY | baseline=STABLE |
| UNHEALTHY | DEAD | pulse=DEAD |
| DEAD | INITIALIZING | reset() called |

---

## 4. The Composition Function

At each step, the composition:

```
timing_step(S_timing, event, timestamp):
    
    1. Compute Δt from previous heartbeat (if available)
       if has_previous_heartbeat:
           Δt = timestamp - last_heartbeat_timestamp
    
    2. Feed event to Pulse
       pulse_result = pulse_step(S_pulse, timestamp, heartbeat_seen=1)
    
    3. If Δt available, feed to Baseline
       if has_Δt:
           baseline_result = baseline_step(S_baseline, Δt)
    
    4. Map component states to composed state
       q_timing = map(pulse_state, baseline_state)
    
    5. Return composed result
       return (q_timing, pulse_result, baseline_result, Δt)
```

### Why This Order?

1. **Δt first:** We need the previous timestamp to compute inter-arrival time
2. **Pulse second:** Updates existence tracking
3. **Baseline third:** Consumes the Δt output from Pulse
4. **Mapping last:** Combines component states

This order ensures data flows correctly: `event → Pulse → Δt → Baseline → state`

---

## 5. Closure Preservation

**Theorem:** If Pulse and Baseline are closed, total, and deterministic, then Timing is closed, total, and deterministic.

### Proof: Closure

A system is **closed** if state depends only on previous state and current input.

Timing state = (Pulse state, Baseline state, mapping result)

- Pulse state depends only on (previous Pulse state, timestamp, heartbeat_seen)
- Baseline state depends only on (previous Baseline state, Δt)
- Mapping is a pure function of current component states

No external state, no history buffers. ✓

### Proof: Totality

A system is **total** if every input produces a valid output.

- Pulse is total → always produces valid state
- Baseline is total → always produces valid result
- Mapping covers all state combinations (see table above)

Therefore, Timing always produces a valid result. ✓

### Proof: Determinism

A system is **deterministic** if same inputs produce same outputs.

- Pulse is deterministic → same (timestamp, heartbeat_seen) → same state
- Baseline is deterministic → same Δt → same state
- Mapping is deterministic → same component states → same timing state

Therefore, same input sequence → same output sequence. ✓

---

## 6. Contract Definitions

### CONTRACT-1: Existence Inheritance

```
If Pulse reports DEAD, Timing reports DEAD.
∀t: (pulse_state(t) = DEAD) → (timing_state(t) = DEAD)
```

The composition never claims health when existence is uncertain.

### CONTRACT-2: Normality Inheritance

```
If Pulse is ALIVE and Baseline reports DEVIATION, Timing reports UNHEALTHY.
∀t: (pulse_state(t) = ALIVE ∧ baseline_state(t) = DEVIATION) → (timing_state(t) = UNHEALTHY)
```

Timing anomalies are surfaced, not hidden.

### CONTRACT-3: Health Requires Evidence

```
Timing reports HEALTHY only when:
  - Pulse has seen ≥ 1 heartbeat
  - Baseline has seen ≥ n_min observations
  - Baseline z-score ≤ k
  
(timing_state = HEALTHY) → (have_hb = 1 ∧ n ≥ n_min ∧ |z| ≤ k)
```

No premature claims of health.

### CONTRACT-4: Bounded Detection Latency

```
A sustained timing anomaly is detected within O(1/α) heartbeats.
```

Inherited from Baseline's sensitivity contract. If Δt values consistently exceed the baseline by >kσ, detection occurs within ~2/α steps.

### CONTRACT-5: Spike Resistance

```
A single anomalous Δt shifts the baseline by at most α·|Δt - μ|.
```

Inherited from Baseline's spike resistance contract. One bad heartbeat can't corrupt the learned baseline.

### CONTRACT-6: Deterministic Composition

```
Given identical event sequences and initial states,
Timing produces identical output sequences.
∀(S₀, I): replay(S₀, I) = replay(S₀, I)
```

The composition is reproducible.

---

## 7. Failure Mode Analysis

### Pulse Fault

If Pulse detects a fault (clock jump, reentry):
- `fault_pulse` flag is set
- Timing state forced to DEAD (fail-safe)

### Baseline Fault

If Baseline detects a fault (NaN, Inf, reentry):
- `fault_baseline` flag is set
- If currently HEALTHY, transition to UNHEALTHY
- Cannot remain HEALTHY with faulted baseline

### Combined Fault

If both components fault:
- All fault flags set
- Timing state forced to DEAD
- Only `reset()` can recover

### Recovery

From DEAD (timeout, not fault):
- New heartbeat → Pulse returns to ALIVE
- Timing returns to INITIALIZING
- Baseline re-learns (or preserves learned baseline, design choice)

---

## 8. Implementation Checklist

Before writing code, verify:

- [ ] State mapping covers all 9 combinations
- [ ] Δt computation handles first heartbeat (no previous)
- [ ] Fault flags propagate correctly
- [ ] Reset clears both components
- [ ] No circular dependencies between components
- [ ] Timestamp ordering is enforced (or detected)

---

## Exercises

### Exercise 2.1: Trace the FSM

Trace the composed FSM for this sequence:
- t=0: Initialize
- t=1000: Heartbeat
- t=2000: Heartbeat
- t=3000: Heartbeat
- ... (repeat 20 times)
- t=25000: No heartbeat for 10 seconds
- t=35000: Heartbeat

Parameters: T=5000ms, W=2000ms, n_min=20, k=3.0

What state is the system in at each significant point?

### Exercise 2.2: Prove Totality

Prove that if Pulse is total, Timing is total.

Hint: What needs to be true about the mapping function?

### Exercise 2.3: Minimum Evidence

What's the minimum number of heartbeats before Timing can report HEALTHY?

Given: n_min=20, W=5000ms

### Exercise 2.4: Fault Propagation

Design the fault propagation logic:
- If Pulse faults, what happens to Timing?
- If Baseline faults while HEALTHY, what happens?
- Can Timing be HEALTHY if any component is faulted?

### Exercise 2.5: Alternative Mapping

Consider an alternative mapping where UNKNOWN × DEVIATION → UNHEALTHY instead of INITIALIZING.

What contract would this violate? Why might someone want this behavior?

---

## Summary

**Key Insight:** The composed state is a pure function of component states. No new state is invented—we're just wiring.

| Component | States | Contribution |
|-----------|--------|--------------|
| Pulse | 3 | Existence evidence |
| Baseline | 3 | Normality evidence |
| Timing | 4 | Combined health |

**Composition preserves:**
- Closure (state depends only on previous state + input)
- Totality (every input produces valid output)
- Determinism (same inputs → same outputs)

---

## Next Lesson

In **Lesson 3: Structs & State Encoding**, we'll translate this mathematical model into C data structures. Every field will trace back to a requirement established here.

---

*"The code is not clever. It's wiring. Pulse emits, Baseline consumes, mapping decides."*

---

[Previous: Lesson 1 — The Problem](../01-the-problem/LESSON.md) | [Next: Lesson 3 — Structs](../03-structs/LESSON.md)
