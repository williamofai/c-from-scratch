# Lesson 3: Structs & State Encoding

## The Composed Struct Contains the Components

> "No pointers, no indirection, no hidden state."

This lesson translates our mathematical model into C data structures. The key insight: we embed component FSMs directly, not references to them.

---

## 1. Configuration: timing_config_t

The configuration combines parameters for both components:

```c
typedef struct {
    /* Pulse configuration */
    uint64_t heartbeat_timeout_ms;  /* T: max time between heartbeats */
    uint64_t init_window_ms;        /* W: max time to first heartbeat */
    
    /* Baseline configuration */
    double   alpha;                 /* EMA smoothing factor ∈ (0,1) */
    double   epsilon;               /* Variance floor for z-score */
    double   k;                     /* Deviation threshold (sigma) */
    uint32_t n_min;                 /* Min observations before STABLE */
} timing_config_t;
```

### Field Justification

| Field | Source | Constraint | Purpose |
|-------|--------|------------|---------|
| `heartbeat_timeout_ms` | Pulse T | > 0 | When to declare DEAD |
| `init_window_ms` | Pulse W | ≥ 0 | Grace period for first heartbeat |
| `alpha` | Baseline α | ∈ (0,1) | EMA smoothing factor |
| `epsilon` | Baseline ε | > 0 | Variance floor |
| `k` | Baseline k | > 0 | Deviation threshold |
| `n_min` | Baseline n_min | ≥ ⌈2/α⌉ | Learning period |

**Note on n_min constraint:** The EMA requires approximately 2/α observations to converge. With α=0.1, the effective window is ~20 observations, so n_min must be ≥ 20. Setting n_min lower would allow claiming STABLE before statistics are meaningful.

**Note on first heartbeat:** The first heartbeat doesn't produce a Δt (no previous timestamp), so you need n_min+1 heartbeats to reach HEALTHY state.

### Default Configuration

```c
static const timing_config_t TIMING_DEFAULT_CONFIG = {
    .heartbeat_timeout_ms = 5000,   /* 5 seconds */
    .init_window_ms       = 10000,  /* 10 seconds to first heartbeat */
    .alpha                = 0.1,    /* Effective window ~20 observations */
    .epsilon              = 1e-9,   /* Variance floor */
    .k                    = 3.0,    /* 3-sigma threshold */
    .n_min                = 20      /* Learning period */
};
```

---

## 2. State Enumeration: timing_state_t

```c
typedef enum {
    TIMING_INITIALIZING = 0,  /* Learning phase */
    TIMING_HEALTHY      = 1,  /* Normal rhythm */
    TIMING_UNHEALTHY    = 2,  /* Timing anomaly */
    TIMING_DEAD         = 3   /* No heartbeat */
} timing_state_t;
```

### Why These Values?

- **Zero-init safe:** `TIMING_INITIALIZING = 0` means a zeroed struct starts in a safe state
- **Severity ordering:** Higher values roughly correspond to more severe conditions
- **Four states only:** Minimal state space that covers all meaningful conditions

---

## 3. The FSM Structure: timing_fsm_t

```c
typedef struct {
    /* Configuration (immutable after init) */
    timing_config_t cfg;
    
    /* Component FSMs (embedded, not referenced) */
    hb_fsm_t   pulse;
    base_fsm_t baseline;
    
    /* Composed state */
    timing_state_t state;
    
    /* Timing tracking */
    uint64_t last_heartbeat_ms;  /* Timestamp of last heartbeat */
    uint8_t  has_prev_heartbeat; /* Have we seen at least one heartbeat? */
    
    /* Fault flags (aggregated from components) */
    uint8_t fault_pulse;         /* Pulse component faulted */
    uint8_t fault_baseline;      /* Baseline component faulted */
    
    /* Atomicity guard */
    uint8_t in_step;
    
    /* Statistics */
    uint32_t heartbeat_count;    /* Total heartbeats observed */
    uint32_t healthy_count;      /* Consecutive healthy observations */
    uint32_t unhealthy_count;    /* Consecutive unhealthy observations */
} timing_fsm_t;
```

### Field-by-Field Justification

| Field | Math Symbol | Purpose | Invariant |
|-------|-------------|---------|-----------|
| `cfg` | — | Configuration snapshot | Immutable after init |
| `pulse` | S_pulse | Pulse FSM state | Embedded, not pointer |
| `baseline` | S_baseline | Baseline FSM state | Embedded, not pointer |
| `state` | q_timing | Composed state | ∈ {0,1,2,3} |
| `last_heartbeat_ms` | t_prev | For Δt computation | Valid after first hb |
| `has_prev_heartbeat` | — | Can we compute Δt? | Boolean |
| `fault_pulse` | — | Pulse fault occurred | Sticky until reset |
| `fault_baseline` | — | Baseline fault occurred | Sticky until reset |
| `in_step` | — | Reentrancy guard | 0 outside step functions |
| `heartbeat_count` | n_hb | Total heartbeats | Monotonic |
| `healthy_count` | — | Consecutive healthy | Reset on state change |
| `unhealthy_count` | — | Consecutive unhealthy | Reset on state change |

---

## 4. Why Embed, Not Reference?

Consider two approaches:

### Approach A: Pointers (Wrong)

```c
typedef struct {
    hb_fsm_t   *pulse;      /* Pointer to pulse */
    base_fsm_t *baseline;   /* Pointer to baseline */
} timing_fsm_t;
```

Problems:
- **Lifetime management:** Who owns the pointed-to structs?
- **Cache unfriendly:** Components may be scattered in memory
- **Hidden state:** The pointer itself is state that must be managed
- **Serialization nightmare:** Can't just copy the struct

### Approach B: Embedding (Correct)

```c
typedef struct {
    hb_fsm_t   pulse;       /* Embedded pulse */
    base_fsm_t baseline;    /* Embedded baseline */
} timing_fsm_t;
```

Benefits:
- **Single allocation:** One malloc (or stack allocation) gets everything
- **Cache friendly:** Components are adjacent in memory
- **Copy safe:** `memcpy(&t2, &t1, sizeof(t))` works
- **Clear ownership:** The composed struct owns its components

**Rule:** Embed components, don't reference them.

---

## 5. Result Structure: timing_result_t

```c
typedef struct {
    /* Composed state */
    timing_state_t state;
    
    /* Inter-arrival time (valid if has_dt is true) */
    double   dt;
    uint8_t  has_dt;
    
    /* Baseline z-score (valid if has_z is true) */
    double   z;
    uint8_t  has_z;
    
    /* Component states (for debugging/logging) */
    state_t      pulse_state;
    base_state_t baseline_state;
    
    /* Convenience flags */
    uint8_t is_healthy;    /* state == TIMING_HEALTHY */
    uint8_t is_unhealthy;  /* state == TIMING_UNHEALTHY */
    uint8_t is_dead;       /* state == TIMING_DEAD */
    uint8_t is_anomaly;    /* is_unhealthy || is_dead */
} timing_result_t;
```

### What's in the Result vs State

**In Result (derived per-call):**
- `dt` — Computed from timestamps
- `z` — Computed by Baseline
- Convenience flags — Computed from state

**In State (persisted):**
- Learned baseline (μ, σ)
- Component states
- Fault flags
- Counters

**Rule:** Results contain derived values. State contains minimal persistent information.

---

## 6. Memory Layout

```
timing_fsm_t total size: ~200 bytes (platform dependent)

Offset  Size    Field
──────  ────    ─────────────────────
0       48      cfg (timing_config_t)
48      32      pulse (hb_fsm_t)
80      88      baseline (base_fsm_t)
168     4       state
172     8       last_heartbeat_ms
180     1       has_prev_heartbeat
181     1       fault_pulse
182     1       fault_baseline
183     1       in_step
184     4       heartbeat_count
188     4       healthy_count
192     4       unhealthy_count
196     4       padding
──────  ────
        ~200 bytes total
```

### O(1) Memory Guarantee

- No dynamic allocation
- No history buffers
- Fixed size regardless of observation count
- Single contiguous block

---

## 7. Invariants

These must hold at all times (checked in tests):

### INV-1: State Range
```
state ∈ { INITIALIZING, HEALTHY, UNHEALTHY, DEAD }
```

### INV-2: Health Implies Components
```
(state == HEALTHY) → (pulse.st == ALIVE ∧ baseline.state == STABLE)
```

### INV-3: Dead Implies Pulse Dead
```
(state == DEAD) → (pulse.st == DEAD)
```

### INV-4: Faults Block Health
```
(fault_pulse ∨ fault_baseline) → (state ≠ HEALTHY)
```

### INV-5: Reentrancy Guard
```
(in_step == 0) when not executing timing_heartbeat/timing_check
```

### INV-6: Timestamp Validity
```
(has_prev_heartbeat == 1) → (last_heartbeat_ms is meaningful)
```

---

## 8. What Is NOT Stored

Intentionally omitted from state:

| Omitted | Reason |
|---------|--------|
| Timestamp history | Violates closure; Baseline uses EMA instead |
| Δt history | Violates closure; derived per-step |
| z-score history | Derived per-step from current Δt |
| Per-call config | Embedded at init time |
| Component pointers | Embedded directly |

---

## Exercises

### Exercise 3.1: Calculate Size

Calculate the exact size of `timing_fsm_t` on your platform.

Hints:
- Check `sizeof(hb_fsm_t)` from Module 1
- Check `sizeof(base_fsm_t)` from Module 2
- Account for alignment/padding

### Exercise 3.2: Embedding vs Referencing

Design an alternative `timing_fsm_t` that uses pointers to components.

What new fields would you need? What functions would need to change?

Why is this worse for safety-critical systems?

### Exercise 3.3: Result Design

The result struct includes both `has_dt` and `dt`. An alternative:

```c
double dt;  /* -1.0 if not available */
```

What are the tradeoffs? Which is safer?

### Exercise 3.4: Convenience Flags

The result includes `is_healthy`, `is_unhealthy`, `is_dead`, `is_anomaly`.

These are redundant (derivable from `state`). Why include them?

### Exercise 3.5: Fault Aggregation

Design the fault flag update logic:
- When should `fault_pulse` be set?
- When should `fault_baseline` be set?
- Can they ever be cleared without `reset()`?

---

## Summary

| Struct | Purpose | Size |
|--------|---------|------|
| `timing_config_t` | Unified configuration | ~48 bytes |
| `timing_state_t` | State enumeration | 4 bytes |
| `timing_fsm_t` | Full composed FSM | ~200 bytes |
| `timing_result_t` | Per-call result | ~40 bytes |

**Key Principles:**
1. Embed components, don't reference them
2. Store minimal state, compute derived values
3. Zero-init yields safe defaults
4. Fixed size, no dynamic allocation

---

## Next Lesson

In **Lesson 4: Code as Composition**, we'll implement `timing_heartbeat()` — the function that wires Pulse output to Baseline input.

---

*"The composed struct contains the components. No pointers, no indirection, no hidden state."*

---

[Previous: Lesson 2 — Mathematical Model](../02-mathematical-model/LESSON.md) | [Next: Lesson 4 — Code](../04-code/LESSON.md)
