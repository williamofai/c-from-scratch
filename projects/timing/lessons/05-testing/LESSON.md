# Lesson 5: Testing Composed Systems

## Test the Components. Test the Composition. Test Them Together Under Chaos.

> "Composition preserves guarantees—but only if you verify it."

Testing a composed system requires testing at multiple levels: individual contracts, integration scenarios, and adversarial conditions.

---

## 1. Test Structure

Our test suite covers three levels:

```c
/* Contract tests - verify each guarantee */
void test_contract1_existence_inheritance(void);
void test_contract2_normality_inheritance(void);
void test_contract3_health_requires_evidence(void);
void test_contract4_bounded_detection(void);
void test_contract5_spike_resistance(void);
void test_contract6_deterministic(void);

/* Integration tests - verify real scenarios */
void test_integration_normal_rhythm(void);
void test_integration_recovery(void);
void test_integration_reset(void);

/* Fuzz tests - stress under chaos */
void test_fuzz_random_timestamps(void);
void test_fuzz_edge_timestamps(void);

/* Config tests - validate parameter checking */
void test_config_validation(void);
```

---

## 2. Contract Tests

### CONTRACT-1: Existence Inheritance

**Guarantee:** If Pulse reports DEAD, Timing reports DEAD.

```c
static void test_contract1_existence_inheritance(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.heartbeat_timeout_ms = 2000;
    cfg.n_min = 20;
    
    timing_init(&t, &cfg);
    
    /* Establish healthy state */
    uint64_t ts = 0;
    for (int i = 0; i < 10; i++) {
        ts += 1000;
        timing_heartbeat(&t, ts);
    }
    
    ASSERT(t.state == TIMING_HEALTHY, "should be healthy");
    
    /* Let it timeout */
    ts += 3000;  /* > heartbeat_timeout_ms */
    timing_result_t r = timing_check(&t, ts);
    
    ASSERT(r.state == TIMING_DEAD, "should be DEAD");
    ASSERT(hb_state(&t.pulse) == STATE_DEAD, "pulse should be DEAD");
    
    PASS("Dead pulse → Dead timing");
}
```

**What this tests:**
- Timeout detection propagates from Pulse to Timing
- DEAD state in Pulse forces DEAD state in Timing
- The composition doesn't mask death

### CONTRACT-2: Normality Inheritance

**Guarantee:** If Pulse is ALIVE and Baseline reports DEVIATION, Timing reports UNHEALTHY.

```c
static void test_contract2_normality_inheritance(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    cfg.k = 2.0;  /* More sensitive */
    
    timing_init(&t, &cfg);
    
    /* Establish baseline at 1000ms */
    uint64_t ts = 0;
    for (int i = 0; i < 15; i++) {
        ts += 1000;
        timing_heartbeat(&t, ts);
    }
    
    ASSERT(t.state == TIMING_HEALTHY, "should be healthy");
    
    /* Inject timing deviation */
    ts += 100;  /* Way too fast */
    timing_result_t r = timing_heartbeat(&t, ts);
    
    ASSERT(hb_state(&t.pulse) == STATE_ALIVE, "pulse should be ALIVE");
    ASSERT(r.state == TIMING_UNHEALTHY, "should be UNHEALTHY");
    
    PASS("Timing deviation → Unhealthy");
}
```

**What this tests:**
- Baseline deviation is detected
- Pulse remains ALIVE (heartbeat was received)
- Composition reports UNHEALTHY, not HEALTHY

### CONTRACT-3: Health Requires Evidence

**Guarantee:** HEALTHY only when sufficient evidence exists.

```c
static void test_contract3_health_requires_evidence(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    
    timing_init(&t, &cfg);
    
    /* Initially INITIALIZING */
    ASSERT(t.state == TIMING_INITIALIZING, "starts INITIALIZING");
    
    /* First heartbeat - still INITIALIZING */
    timing_result_t r = timing_heartbeat(&t, 1000);
    ASSERT(r.state == TIMING_INITIALIZING, "one heartbeat not enough");
    
    /* Before n_min observations */
    uint64_t ts = 1000;
    for (int i = 0; i < 5; i++) {
        ts += 1000;
        r = timing_heartbeat(&t, ts);
    }
    ASSERT(r.state == TIMING_INITIALIZING, "before n_min");
    
    /* After n_min observations */
    for (int i = 0; i < 10; i++) {
        ts += 1000;
        r = timing_heartbeat(&t, ts);
    }
    ASSERT(r.state == TIMING_HEALTHY, "after n_min with stable timing");
    
    PASS("Health requires evidence");
}
```

**What this tests:**
- Can't claim HEALTHY immediately
- Learning period is enforced
- HEALTHY only after sufficient observations

### CONTRACT-4: Bounded Detection Latency

**Guarantee:** Sustained anomalies detected within O(1/α) steps.

```c
static void test_contract4_bounded_detection(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    cfg.alpha = 0.1;
    cfg.k = 2.0;  /* Lower threshold for faster detection */
    
    timing_init(&t, &cfg);
    
    /* Establish baseline at 1000ms */
    uint64_t ts = 0;
    for (int i = 0; i < 20; i++) {
        ts += 1000;
        timing_heartbeat(&t, ts);
    }
    
    /* Switch to 2000ms (sustained anomaly) */
    int steps_to_detect = 0;
    
    for (int i = 0; i < 100; i++) {
        ts += 2000;
        timing_result_t r = timing_heartbeat(&t, ts);
        steps_to_detect++;
        
        if (r.state == TIMING_UNHEALTHY) {
            break;
        }
    }
    
    int expected_bound = (int)ceil(2.0 / cfg.alpha);  /* ~20 steps */
    ASSERT(steps_to_detect <= expected_bound + 5, "detection bounded");
    
    PASS("Bounded detection latency");
}
```

**What this tests:**
- Sustained anomalies are eventually detected
- Detection time is bounded by O(1/α)
- EMA convergence works as expected

### CONTRACT-5: Spike Resistance

**Guarantee:** Single outlier shifts baseline by at most α·|spike|.

```c
static void test_contract5_spike_resistance(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    cfg.alpha = 0.1;
    cfg.k = 5.0;  /* High threshold */
    
    timing_init(&t, &cfg);
    
    /* Establish baseline at 1000ms */
    uint64_t ts = 0;
    for (int i = 0; i < 20; i++) {
        ts += 1000;
        timing_heartbeat(&t, ts);
    }
    
    double mu_before = t.baseline.mu;
    
    /* Single large spike */
    ts += 5000;
    timing_heartbeat(&t, ts);
    
    double mu_after = t.baseline.mu;
    double delta_mu = fabs(mu_after - mu_before);
    double max_shift = cfg.alpha * fabs(5000.0 - mu_before);
    
    ASSERT(delta_mu <= max_shift * 1.01, "shift bounded");
    
    /* Return to normal */
    for (int i = 0; i < 10; i++) {
        ts += 1000;
        timing_heartbeat(&t, ts);
    }
    
    ASSERT(t.state == TIMING_HEALTHY, "recovers from spike");
    
    PASS("Spike resistance verified");
}
```

**What this tests:**
- Single outlier doesn't corrupt baseline
- EMA dampens spikes
- System recovers after transient anomaly

### CONTRACT-6: Deterministic Composition

**Guarantee:** Same inputs → same outputs.

```c
static void test_contract6_deterministic(void) {
    timing_fsm_t t1, t2;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    
    timing_init(&t1, &cfg);
    timing_init(&t2, &cfg);
    
    /* Identical sequences */
    uint64_t timestamps[] = {1000, 2000, 3000, 4000, 5000, 
                             6000, 7000, 8000, 9000, 10000,
                             11000, 12000, 10500, 13000};
    int n = sizeof(timestamps) / sizeof(timestamps[0]);
    
    for (int i = 0; i < n; i++) {
        timing_result_t r1 = timing_heartbeat(&t1, timestamps[i]);
        timing_result_t r2 = timing_heartbeat(&t2, timestamps[i]);
        
        ASSERT(r1.state == r2.state, "states match");
        if (r1.has_dt) {
            ASSERT(fabs(r1.dt - r2.dt) < 0.001, "dt matches");
        }
        if (r1.has_z) {
            ASSERT(fabs(r1.z - r2.z) < 0.001, "z matches");
        }
    }
    
    ASSERT(t1.state == t2.state, "final states match");
    
    PASS("Identical inputs → identical outputs");
}
```

**What this tests:**
- Two FSMs fed identical inputs produce identical outputs
- No hidden state or randomness
- Composition is reproducible

---

## 3. Integration Tests

### Normal Operation

```c
static void test_integration_normal_rhythm(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    
    timing_init(&t, &cfg);
    
    uint64_t ts = 0;
    int healthy_count = 0;
    
    for (int i = 0; i < 40; i++) {
        ts += 1000;
        timing_result_t r = timing_heartbeat(&t, ts);
        if (r.state == TIMING_HEALTHY) {
            healthy_count++;
        }
    }
    
    ASSERT(healthy_count >= 15, "mostly healthy");
    ASSERT(t.state == TIMING_HEALTHY, "ends healthy");
    
    PASS("Normal rhythm stays healthy");
}
```

### Recovery from Anomaly

```c
static void test_integration_recovery(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    cfg.k = 2.0;  /* More sensitive */
    
    timing_init(&t, &cfg);
    
    /* Establish baseline */
    uint64_t ts = 0;
    for (int i = 0; i < 25; i++) {
        ts += 1000;
        timing_heartbeat(&t, ts);
    }
    
    ASSERT(t.state == TIMING_HEALTHY, "starts healthy");
    
    /* Trigger deviation - very short interval */
    ts += 50;
    timing_result_t r = timing_heartbeat(&t, ts);
    ASSERT(r.state == TIMING_UNHEALTHY, "detects anomaly");
    
    /* Return to normal */
    for (int i = 0; i < 25; i++) {
        ts += 1000;
        r = timing_heartbeat(&t, ts);
    }
    
    ASSERT(t.state == TIMING_HEALTHY, "recovers");
    
    PASS("Recovers from temporary anomaly");
}
```

---

## 4. Fuzz Tests

### Random Timestamps

```c
static void test_fuzz_random_timestamps(void) {
    timing_fsm_t t;
    timing_init(&t, &TIMING_DEFAULT_CONFIG);
    
    srand(12345);
    uint64_t ts = 0;
    int valid_states = 0;
    
    for (int i = 0; i < 10000; i++) {
        uint64_t interval = (uint64_t)(100 + rand() % 2900);
        ts += interval;
        
        timing_result_t r = timing_heartbeat(&t, ts);
        
        if (r.state >= TIMING_INITIALIZING && 
            r.state <= TIMING_DEAD) {
            valid_states++;
        }
        
        ASSERT(t.in_step == 0, "reentrancy guard clear");
    }
    
    ASSERT(valid_states == 10000, "all states valid");
    
    PASS("10000 random timestamps handled");
}
```

**What this tests:**
- No crashes under random input
- State always valid
- Invariants maintained

### Edge Case Timestamps

```c
static void test_fuzz_edge_timestamps(void) {
    timing_fsm_t t;
    timing_init(&t, &TIMING_DEFAULT_CONFIG);
    
    uint64_t edges[] = {0, 1, 1000, UINT32_MAX, UINT64_MAX/2};
    
    uint64_t prev = 0;
    for (int i = 0; i < sizeof(edges)/sizeof(edges[0]); i++) {
        if (edges[i] >= prev) {
            timing_result_t r = timing_heartbeat(&t, edges[i]);
            ASSERT(r.state <= TIMING_DEAD, "valid state");
            prev = edges[i];
        }
    }
    
    PASS("Edge timestamps handled");
}
```

---

## 5. Configuration Validation Tests

```c
static void test_config_validation(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    
    /* Valid config */
    ASSERT(timing_init(&t, &cfg) == 0, "valid succeeds");
    
    /* Zero timeout */
    cfg = TIMING_DEFAULT_CONFIG;
    cfg.heartbeat_timeout_ms = 0;
    ASSERT(timing_init(&t, &cfg) == -1, "zero timeout fails");
    
    /* Alpha out of range */
    cfg = TIMING_DEFAULT_CONFIG;
    cfg.alpha = 0.0;
    ASSERT(timing_init(&t, &cfg) == -1, "alpha=0 fails");
    
    cfg.alpha = 1.0;
    ASSERT(timing_init(&t, &cfg) == -1, "alpha=1 fails");
    
    /* n_min too small */
    cfg = TIMING_DEFAULT_CONFIG;
    cfg.alpha = 0.1;
    cfg.n_min = 10;  /* Should be >= 20 for alpha=0.1 */
    ASSERT(timing_init(&t, &cfg) == -1, "small n_min fails");
    
    PASS("Config validation works");
}
```

---

## 6. Test Output Format

```
╔═════════════════════════════════════════════════════════════╗
║           Timing Monitor - Contract Tests                   ║
╚═════════════════════════════════════════════════════════════╝

--- Contract Tests ---
[PASS] test_contract1_existence_inheritance: Dead pulse → Dead timing
[PASS] test_contract2_normality_inheritance: Timing deviation → Unhealthy
[PASS] test_contract3_health_requires_evidence: Health requires evidence
[PASS] test_contract4_bounded_detection: Detected in 8 steps (bound ~20)
[PASS] test_contract5_spike_resistance: Shift=400.0 <= max=400.0, recovered
[PASS] test_contract6_deterministic: Identical inputs → identical outputs

--- Integration Tests ---
[PASS] test_integration_normal_rhythm: Normal rhythm stays healthy
[PASS] test_integration_recovery: Recovers from temporary anomaly
[PASS] test_integration_reset: Reset clears state

--- Fuzz Tests ---
[PASS] test_fuzz_random_timestamps: 10000 random timestamps
[PASS] test_fuzz_edge_timestamps: Edge cases handled

--- Config Validation Tests ---
[PASS] test_config_validation: Config validation works

═══════════════════════════════════════════════════════════════
  Results: 12/12 tests passed
═══════════════════════════════════════════════════════════════
```

---

## 7. Hardening Checklist

Before declaring the module complete:

- [ ] All 6 contracts have passing tests
- [ ] Integration tests cover normal operation
- [ ] Fuzz tests run 10,000+ iterations without crash
- [ ] Config validation rejects all invalid combinations
- [ ] Memory: `sizeof(timing_fsm_t)` is constant
- [ ] No dynamic allocation in any code path
- [ ] Reset clears faults and statistics
- [ ] Determinism verified with parallel FSMs

---

## Exercises

### Exercise 5.1: Add a Contract Test

Write a test for this property:

> "After reset(), the system returns to INITIALIZING state."

### Exercise 5.2: Fault Injection Test

Write a test that:
1. Establishes HEALTHY state
2. Triggers a baseline fault (e.g., via NaN injection if possible)
3. Verifies fault flag is set
4. Verifies state is no longer HEALTHY

### Exercise 5.3: Stress Test

Write a test that:
1. Runs 100,000 observations
2. Alternates between normal and anomalous timing
3. Verifies invariants hold throughout
4. Measures time to detect each anomaly period

### Exercise 5.4: Concurrent Access Test

Design (but don't implement) a test for concurrent access:
- Two threads calling `timing_heartbeat()` simultaneously
- How would the reentrancy guard behave?
- What would the expected result be?

---

## Summary

| Test Level | What It Verifies |
|------------|-----------------|
| Contract | Individual guarantees |
| Integration | Real-world scenarios |
| Fuzz | Stability under chaos |
| Config | Parameter validation |

**Testing Principle:**
> Trust, but verify. The composition should preserve guarantees—but only testing confirms it.

---

## Next Lesson

In **Lesson 6: Applications & Extensions**, we'll explore where this pattern applies and how it scales to larger systems.

---

*"Test the components. Test the composition. Test them together under chaos."*

---

[Previous: Lesson 4 — Code](../04-code/LESSON.md) | [Next: Lesson 6 — Applications](../06-applications/LESSON.md)
