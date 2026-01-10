# c-from-scratch — Module 7: Mode Manager

## Lesson 2: Mathematical Model

---

## State Machine Definition

The Mode Manager is a **Deterministic Finite State Machine (FSM)**:

```
M = (S, Σ, δ, s₀, F)

Where:
  S  = { INIT, STARTUP, OPERATIONAL, DEGRADED, EMERGENCY, TEST }
  Σ  = Health states from 6 modules + flags
  δ  = Transition function
  s₀ = INIT
  F  = { OPERATIONAL }  (desired steady state)
```

---

## Transition Function

The transition function δ: S × Σ → S is defined by these rules:

### Priority 1: Fault Detection (Highest)
```
∀s ∈ S \ {EMERGENCY, TEST}:
  if any module reports FAULTY → EMERGENCY
```

### Priority 2: State-Specific Logic

**From INIT:**
```
if all modules OK or LEARNING → STARTUP
```

**From STARTUP:**
```
if all HEALTHY ∧ no flags ∧ dwell ≥ min → OPERATIONAL
if any DEGRADED → DEGRADED
```

**From OPERATIONAL:**
```
if any DEGRADED → DEGRADED
if any critical flag → DEGRADED
```

**From DEGRADED:**
```
if all HEALTHY ∧ no flags ∧ dwell ≥ min → OPERATIONAL
```

**From EMERGENCY:**
```
∅ (no automatic transitions — requires reset)
```

---

## Forbidden Transitions

These transitions are **impossible**:

| From | To | Reason |
|------|----|--------|
| INIT | OPERATIONAL | Must learn first |
| EMERGENCY | * (without reset) | Fault is sticky |
| OPERATIONAL | STARTUP | Cannot regress |
| DEGRADED | STARTUP | Cannot regress |

---

## Hysteresis (Dwell Time)

To prevent mode flapping from noisy sensors:

```
min_dwell_startup  = n cycles in STARTUP before OPERATIONAL
min_dwell_degraded = m cycles in DEGRADED before recovery
```

A single glitch shouldn't bounce between modes.

---

## Value-Awareness

Beyond state, we consider **semantic flags**:

```
flags = {
  approaching_upper,   /* Drift: TTF < threshold */
  approaching_lower,
  low_confidence,      /* Consensus: conf < 50% */
  queue_critical,      /* Pressure: fill > 90% */
  timing_unstable,     /* Timing: recent jitter */
  baseline_volatile    /* Baseline: high deviation */
}
```

These enable **proactive safety**:
- State-only: "The sensor failed" (reactive)
- Value-aware: "The sensor is ABOUT to fail" (proactive)

---

## Contracts

| Contract | Formal Statement |
|----------|------------------|
| C1: Unambiguous | ∀t: |{m ∈ S : active(m,t)}| = 1 |
| C2: Safe Entry | mode = OPERATIONAL → ∀i: state[i] = HEALTHY |
| C3: Sticky Fault | mode = EMERGENCY → requires reset() to exit |
| C4: No Skip | INIT ↛ OPERATIONAL (must pass through STARTUP) |
| C5: Bounded Latency | FAULTY detected → EMERGENCY in ≤ 1 cycle |
| C6: Deterministic | δ is a function (single-valued) |
| C7: Proactive | critical flag → DEGRADED (before fault) |
| C8: Auditable | ∀ transitions: logged with timestamp and cause |

---

## Invariants

| Invariant | Property |
|-----------|----------|
| INV-1 | mode ∈ S (always valid) |
| INV-2 | OPERATIONAL → all healthy ∧ no flags |
| INV-3 | EMERGENCY → fault_active |
| INV-4 | ticks_in_mode monotonic until transition |

---

## Permissions Matrix

Actions are constrained by mode:

| Mode | Actuate | Calibrate | Log | Communicate |
|------|---------|-----------|-----|-------------|
| INIT | ✗ | ✗ | ✓ | ✓ |
| STARTUP | ✗ | ✓ | ✓ | ✓ |
| OPERATIONAL | ✓ | ✓ | ✓ | ✓ |
| DEGRADED | ✗ | ✗ | ✓ | ✓ |
| EMERGENCY | ✗ | ✗ | ✓ | ✓ |
| TEST | ✓ | ✓ | ✓ | ✓ |

---

## Next Lesson

We encode this model in C structs: modes, flags, transitions, and the audit log.
