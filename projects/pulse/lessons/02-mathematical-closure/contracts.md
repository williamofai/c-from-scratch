# Contracts Reference

## Safety Contracts for Heartbeat Liveness Monitor

This document defines the formal contracts that the implementation **must** satisfy. These are not guidelines—they are provable properties.

---

## CONTRACT-1: Soundness

### Statement
> The system shall **never** report ALIVE if the monitored process is actually dead.

### Formal Definition
```
∀t: (st = ALIVE at time t) → ∃t' ∈ [t-T, t]: heartbeat received at t'
```

If we report ALIVE, there must have been a heartbeat within the last T time units.

### Why It Matters
A false ALIVE is catastrophic: a dead process goes unnoticed, potentially causing data loss, service outage, or safety hazard.

### How We Guarantee It
1. ALIVE requires `have_hb = 1` (evidence exists)
2. ALIVE requires `a_hb ≤ T` (evidence is fresh)
3. Invalid time arithmetic forces DEAD (fail-safe)

### Test Strategy
- Inject scenarios where process dies; verify ALIVE never reported after T
- Inject clock corruption; verify DEAD reported

---

## CONTRACT-2: Liveness  

### Statement
> The system shall **eventually** report DEAD if heartbeats permanently stop.

### Formal Definition
```
∀t*: (last heartbeat at t* ∧ no heartbeats after t*)
     → ∃t ≤ t* + T + P: st = DEAD at time t
```

If heartbeats stop at t*, DEAD will be reported within T + P time units.

### Why It Matters
A stuck UNKNOWN or ALIVE state means the monitor itself has failed. We need bounded detection time.

### How We Guarantee It
1. Age computation eventually yields `a_hb > T`
2. Polling occurs at least every P time units
3. `a_hb > T` → DEAD (transition table row 6)

### Pre-Condition
This contract assumes the caller invokes `hb_step` at least every P time units. If polling stops, the contract is void.

### Test Strategy
- Stop heartbeats; verify DEAD within T + P
- Verify DEAD timing across boundary conditions (T-1, T, T+1)

---

## CONTRACT-3: Stability

### Statement
> The system shall **not** transition state without cause.

### Formal Definition (Strong Form)
```
∀t: (st at t) = (st at t-1) unless:
  - hb_seen changed, or
  - age crossed threshold T, or  
  - fault detected
```

### Formal Definition (Weak Form)
```
∀t: |t_dead - (t* + T)| ≤ T_margin
```

DEAD is reported within margin of the actual timeout.

### Why It Matters
Spurious transitions cause unnecessary restarts (DEAD→ALIVE→DEAD flapping) or false confidence (UNKNOWN→ALIVE without evidence).

### How We Guarantee It
1. Each transition has explicit cause (see transition table)
2. No "maybe" or "random" branches in logic
3. Deterministic: same inputs always produce same outputs

### Test Strategy
- Fuzz test: random hb_seen patterns, verify all transitions are justified
- Edge cases: T-1, T, T+1 timing

---

## Contract Relationships

```
     CONTRACT-1 (Soundness)
           │
           │ "If we say ALIVE, we're right"
           │
           ▼
    ┌──────────────┐
    │  ALIVE state │◄──── Evidence required
    └──────────────┘
           │
           │ CONTRACT-2 (Liveness)
           │ "If dead, we eventually say DEAD"
           │
           ▼
    ┌──────────────┐
    │  DEAD state  │◄──── Timeout or fault
    └──────────────┘
           │
           │ CONTRACT-3 (Stability)
           │ "We don't flip-flop without reason"
           │
           ▼
    All transitions justified
```

---

## Assumptions (Pre-Conditions)

The contracts hold **only if** these assumptions are met:

### A1: Single-Writer
Only one execution context calls `hb_step` at a time. Concurrent calls break atomicity.

### A2: Monotonic Time (or Fault Tolerance)
Time source is monotonic. If not, clock faults will be detected and handled via fail-safe (DEAD).

### A3: Bounded Polling
`hb_step` is called at least every P time units. If not, CONTRACT-2 bound does not hold.

### A4: Valid Parameters
- T > 0
- W ≥ 0
- P ≤ T (for meaningful liveness bound)

---

## Failure Modes

When assumptions are violated, contracts may not hold. Here's what happens:

| Violation | Contract Impact | System Response |
|-----------|-----------------|-----------------|
| Concurrent access | All contracts | `fault_reentry`, force DEAD |
| Clock jump | CONTRACT-1,2 timing | `fault_time`, force DEAD |
| Polling stops | CONTRACT-2 bound | DEAD delayed (no bound) |
| T = 0 | CONTRACT-2 | Immediate DEAD always |
| Invalid state | All contracts | Caught by transition logic |

---

## Contract Verification Checklist

For each contract, verify during testing:

### CONTRACT-1 Verification
- [ ] No path to ALIVE without `have_hb = 1`
- [ ] No path to ALIVE with `a_hb > T`
- [ ] Clock corruption forces DEAD
- [ ] Reentry forces DEAD

### CONTRACT-2 Verification
- [ ] Heartbeat stop → DEAD within T + P
- [ ] Verified at boundaries: T-1 (ALIVE), T (edge), T+1 (DEAD)
- [ ] Works with W = 0 and W > 0

### CONTRACT-3 Verification
- [ ] Every transition maps to table row
- [ ] No transition without input change or time crossing threshold
- [ ] Fuzz testing shows no unexplained transitions

---

## Mathematical Summary

```
State Space:     S = { UNKNOWN, ALIVE, DEAD }
Time Domain:     𝕋 = ℤ₂⁶⁴
Input Space:     I = 𝕋 × {0,1}  (now, hb_seen)
Transition:      δ: S × I → S   (total function)

CONTRACT-1:      ALIVE → ∃ fresh evidence
CONTRACT-2:      ¬heartbeat → ◇DEAD  (eventually)
CONTRACT-3:      st' ≠ st → justified cause
```

Where:
- `◇` means "eventually" (temporal logic)
- `→` means "implies"
- `∃` means "there exists"
