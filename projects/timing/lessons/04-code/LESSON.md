# Lesson 4: Code as Composition

## The Code Is Not Clever. It's Wiring.

> "Pulse emits, Baseline consumes, mapping decides."

This lesson shows how the mathematical composition translates directly into C code. The implementation is deliberately simple—the complexity was handled in the design phase.

---

## 1. The State Mapping Function

This is the heart of the composition—a pure function:

```c
static timing_state_t map_states(state_t pulse_st, base_state_t baseline_st) {
    /* Dead pulse always means dead timing (CONTRACT-1) */
    if (pulse_st == STATE_DEAD) {
        return TIMING_DEAD;
    }
    
    /* Unknown pulse means we're still initializing */
    if (pulse_st == STATE_UNKNOWN) {
        return TIMING_INITIALIZING;
    }
    
    /* Pulse is ALIVE - check baseline state */
    switch (baseline_st) {
        case BASE_LEARNING:
            return TIMING_INITIALIZING;
        case BASE_STABLE:
            return TIMING_HEALTHY;
        case BASE_DEVIATION:
            return TIMING_UNHEALTHY;
        default:
            /* Invalid baseline state - fail-safe */
            return TIMING_UNHEALTHY;
    }
}
```

### Why This Structure?

1. **DEAD checked first:** Most important condition, checked first
2. **UNKNOWN blocks health:** Can't claim healthy without existence
3. **Switch on baseline:** Clean enumeration of remaining cases
4. **Default fails safe:** Unknown states → UNHEALTHY, not HEALTHY

### Mapping Table Verification

Cross-reference with Lesson 2:

| Pulse | Baseline | map_states() returns |
|-------|----------|---------------------|
| DEAD | * | TIMING_DEAD ✓ |
| UNKNOWN | * | TIMING_INITIALIZING ✓ |
| ALIVE | LEARNING | TIMING_INITIALIZING ✓ |
| ALIVE | STABLE | TIMING_HEALTHY ✓ |
| ALIVE | DEVIATION | TIMING_UNHEALTHY ✓ |

The code exactly matches the mathematical specification.

---

## 2. Initialization

```c
int timing_init(timing_fsm_t *t, const timing_config_t *cfg) {
    if (!t || !cfg) {
        return -1;
    }
    
    /* Validate Pulse configuration */
    if (cfg->heartbeat_timeout_ms == 0) {
        return -1;
    }
    
    /* Validate Baseline configuration */
    if (cfg->alpha <= 0.0 || cfg->alpha >= 1.0) {
        return -1;
    }
    if (cfg->epsilon <= 0.0) {
        return -1;
    }
    if (cfg->k <= 0.0) {
        return -1;
    }
    
    /* n_min must be at least ceil(2/alpha) for EMA convergence */
    uint32_t min_n_min = (uint32_t)ceil(2.0 / cfg->alpha);
    if (cfg->n_min < min_n_min) {
        return -1;
    }
    
    /* Store configuration */
    t->cfg = *cfg;
    
    /* Initialize Pulse component */
    hb_init(&t->pulse, 0);
    
    /* Initialize Baseline component */
    base_config_t base_cfg = {
        .alpha   = cfg->alpha,
        .epsilon = cfg->epsilon,
        .k       = cfg->k,
        .n_min   = cfg->n_min
    };
    if (base_init(&t->baseline, &base_cfg) != 0) {
        return -1;
    }
    
    /* Initialize composed state */
    t->state = TIMING_INITIALIZING;
    t->last_heartbeat_ms = 0;
    t->has_prev_heartbeat = 0;
    
    /* Clear fault flags */
    t->fault_pulse = 0;
    t->fault_baseline = 0;
    
    /* Clear atomicity guard */
    t->in_step = 0;
    
    /* Clear statistics */
    t->heartbeat_count = 0;
    t->healthy_count = 0;
    t->unhealthy_count = 0;
    
    return 0;
}
```

### Key Points

1. **Validate all inputs:** Reject invalid configuration upfront
2. **Copy configuration:** Don't hold references
3. **Initialize components:** Each component initializes independently
4. **Zero statistics:** Clean slate

---

## 3. The Main Composition: timing_heartbeat()

This is where the composition happens:

```c
timing_result_t timing_heartbeat(timing_fsm_t *t, uint64_t timestamp_ms) {
    timing_result_t result = {0};
    double dt = 0.0;
    uint8_t has_dt = 0;
    double z = 0.0;
    uint8_t has_z = 0;
    
    if (!t) {
        result.state = TIMING_DEAD;
        result.is_dead = 1;
        result.is_anomaly = 1;
        return result;
    }
    
    /* Reentrancy guard */
    if (t->in_step) {
        t->fault_pulse = 1;
        t->state = TIMING_DEAD;
        return build_result(t, 0, 0, 0, 0);
    }
    t->in_step = 1;
    
    /* Step 1: Compute inter-arrival time (Δt) */
    if (t->has_prev_heartbeat) {
        uint64_t elapsed = timestamp_ms - t->last_heartbeat_ms;
        dt = (double)elapsed;
        has_dt = 1;
    }
    
    /* Step 2: Feed heartbeat to Pulse component */
    hb_step(&t->pulse, timestamp_ms, 1,
            t->cfg.heartbeat_timeout_ms, t->cfg.init_window_ms);
    
    /* Check for Pulse fault */
    if (hb_faulted(&t->pulse)) {
        t->fault_pulse = 1;
    }
    
    /* Step 3: If we have Δt, feed to Baseline component */
    if (has_dt && !t->fault_pulse) {
        base_result_t base_r = base_step(&t->baseline, dt);
        z = base_r.z;
        has_z = 1;
        
        /* Check for Baseline fault */
        if (base_faulted(&t->baseline)) {
            t->fault_baseline = 1;
        }
    }
    
    /* Step 4: Map component states to timing state */
    state_t pulse_st = hb_state(&t->pulse);
    base_state_t baseline_st = base_state(&t->baseline);
    
    timing_state_t new_state = map_states(pulse_st, baseline_st);
    
    /* Handle faults: force to safe state */
    if (t->fault_pulse) {
        new_state = TIMING_DEAD;
    } else if (t->fault_baseline && new_state == TIMING_HEALTHY) {
        new_state = TIMING_UNHEALTHY;
    }
    
    /* Update state and statistics */
    t->state = new_state;
    t->last_heartbeat_ms = timestamp_ms;
    t->has_prev_heartbeat = 1;
    t->heartbeat_count++;
    
    /* Update consecutive counters */
    if (new_state == TIMING_HEALTHY) {
        t->healthy_count++;
        t->unhealthy_count = 0;
    } else if (new_state == TIMING_UNHEALTHY) {
        t->unhealthy_count++;
        t->healthy_count = 0;
    } else {
        t->healthy_count = 0;
        t->unhealthy_count = 0;
    }
    
    t->in_step = 0;
    
    /* Step 5: Build and return result */
    return build_result(t, dt, has_dt, z, has_z);
}
```

### The Five Steps

Let's trace the composition:

```
Step 1: Compute Δt
        timestamp_ms - last_heartbeat_ms = dt
        
Step 2: Feed to Pulse
        hb_step(pulse, timestamp, heartbeat_seen=1)
        
Step 3: Feed Δt to Baseline
        base_step(baseline, dt) → z-score
        
Step 4: Map states
        map_states(pulse_state, baseline_state) → timing_state
        
Step 5: Return result
        Package everything for caller
```

This is the composition pattern: **output of one component becomes input of the next**.

---

## 4. Timeout Check: timing_check()

Sometimes we need to check for timeout without a heartbeat event:

```c
timing_result_t timing_check(timing_fsm_t *t, uint64_t current_time_ms) {
    if (!t) {
        timing_result_t result = {0};
        result.state = TIMING_DEAD;
        result.is_dead = 1;
        result.is_anomaly = 1;
        return result;
    }
    
    /* Reentrancy guard */
    if (t->in_step) {
        t->fault_pulse = 1;
        t->state = TIMING_DEAD;
        return build_result(t, 0, 0, 0, 0);
    }
    t->in_step = 1;
    
    /* Check pulse timeout (no heartbeat seen) */
    hb_step(&t->pulse, current_time_ms, 0,  /* heartbeat_seen = 0 */
            t->cfg.heartbeat_timeout_ms, t->cfg.init_window_ms);
    
    /* Check for Pulse fault */
    if (hb_faulted(&t->pulse)) {
        t->fault_pulse = 1;
    }
    
    /* Map states (baseline unchanged) */
    state_t pulse_st = hb_state(&t->pulse);
    base_state_t baseline_st = base_state(&t->baseline);
    
    timing_state_t new_state = map_states(pulse_st, baseline_st);
    
    /* Handle faults */
    if (t->fault_pulse) {
        new_state = TIMING_DEAD;
    }
    
    t->state = new_state;
    
    t->in_step = 0;
    
    return build_result(t, 0, 0, 0, 0);
}
```

### Key Difference from timing_heartbeat()

- **No Δt computation:** No heartbeat event occurred
- **Pulse gets heartbeat_seen=0:** Checks for timeout
- **Baseline unchanged:** No new observation to process

This allows detecting DEAD state even when heartbeats stop.

---

## 5. Reset

```c
void timing_reset(timing_fsm_t *t) {
    if (!t) return;
    
    /* Re-initialize Pulse */
    hb_init(&t->pulse, 0);
    
    /* Reset Baseline */
    base_reset(&t->baseline);
    
    /* Reset composed state */
    t->state = TIMING_INITIALIZING;
    t->last_heartbeat_ms = 0;
    t->has_prev_heartbeat = 0;
    
    /* Clear faults */
    t->fault_pulse = 0;
    t->fault_baseline = 0;
    
    /* Clear statistics */
    t->heartbeat_count = 0;
    t->healthy_count = 0;
    t->unhealthy_count = 0;
}
```

Reset clears everything, including faults. The learned baseline is lost.

---

## 6. Fault Handling

### Reentrancy Detection

```c
if (t->in_step) {
    t->fault_pulse = 1;
    t->state = TIMING_DEAD;
    return build_result(t, 0, 0, 0, 0);
}
t->in_step = 1;
/* ... processing ... */
t->in_step = 0;
```

If someone calls `timing_heartbeat()` from within `timing_heartbeat()` (e.g., signal handler), we detect it and fail safe.

### Component Fault Propagation

```c
/* Check for Pulse fault */
if (hb_faulted(&t->pulse)) {
    t->fault_pulse = 1;
}

/* Check for Baseline fault */
if (base_faulted(&t->baseline)) {
    t->fault_baseline = 1;
}
```

Faults bubble up from components to the composition.

### Fault Effect on State

```c
/* Handle faults: force to safe state */
if (t->fault_pulse) {
    new_state = TIMING_DEAD;
} else if (t->fault_baseline && new_state == TIMING_HEALTHY) {
    new_state = TIMING_UNHEALTHY;
}
```

- Pulse fault → DEAD (can't trust existence)
- Baseline fault → UNHEALTHY (can't trust normality)

---

## 7. The Result Builder

```c
static timing_result_t build_result(const timing_fsm_t *t,
                                     double dt, uint8_t has_dt,
                                     double z, uint8_t has_z) {
    timing_result_t r;
    
    r.state = t->state;
    r.dt = dt;
    r.has_dt = has_dt;
    r.z = z;
    r.has_z = has_z;
    
    r.pulse_state = hb_state(&t->pulse);
    r.baseline_state = base_state(&t->baseline);
    
    r.is_healthy = (t->state == TIMING_HEALTHY);
    r.is_unhealthy = (t->state == TIMING_UNHEALTHY);
    r.is_dead = (t->state == TIMING_DEAD);
    r.is_anomaly = r.is_unhealthy || r.is_dead;
    
    return r;
}
```

All derived values computed at the end, not stored in state.

---

## 8. What Makes This "Composition"?

The code structure mirrors the mathematical composition:

```
Mathematical:     event → Pulse → Δt → Baseline → state
     
Code:             heartbeat → hb_step → dt → base_step → map_states
```

Each component:
1. Has its own init/step/reset
2. Maintains its own state
3. Knows nothing about the other component

The composition:
1. Wires outputs to inputs
2. Maps component states to composed state
3. Aggregates faults

**The code is literally just wiring.**

---

## Exercises

### Exercise 4.1: Implement timing_heartbeat()

Starting from the mathematical spec in Lesson 2, implement `timing_heartbeat()` without looking at the solution.

Compare your implementation to the one shown here.

### Exercise 4.2: Timestamp Ordering

What happens if timestamps decrease? (e.g., `timing_heartbeat(t, 1000)` then `timing_heartbeat(t, 500)`)

Trace through the code. What would `dt` be? Is this a problem?

### Exercise 4.3: Fault Propagation

Design a test that:
1. Establishes HEALTHY state
2. Injects a Pulse fault (e.g., clock jump)
3. Verifies state becomes DEAD
4. Verifies fault flag is sticky
5. Resets and verifies recovery

### Exercise 4.4: First Heartbeat

Trace `timing_heartbeat()` for the very first call:
- What is `has_prev_heartbeat`?
- Is `dt` computed?
- Is `base_step()` called?
- What state results?

### Exercise 4.5: Why timing_check()?

Why do we need `timing_check()` separate from `timing_heartbeat()`?

What happens if we only call `timing_heartbeat()` and heartbeats stop?

---

## Summary

| Function | Purpose | Key Point |
|----------|---------|-----------|
| `map_states()` | Pure state mapping | No side effects |
| `timing_init()` | Initialize composition | Validates config |
| `timing_heartbeat()` | Main composition | Wires components |
| `timing_check()` | Timeout detection | No new observation |
| `timing_reset()` | Full reset | Clears faults |

**The composition pattern:**
```
Component A output → Component B input → Mapping → Result
```

---

## Next Lesson

In **Lesson 5: Testing Composed Systems**, we'll verify all six contracts with rigorous tests, including fuzz testing the composition.

---

*"The code is not clever. It's wiring. Pulse emits, Baseline consumes, mapping decides."*

---

[Previous: Lesson 3 — Structs](../03-structs/LESSON.md) | [Next: Lesson 5 — Testing](../05-testing/LESSON.md)
