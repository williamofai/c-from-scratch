# Lesson 6: Applications & Extensions

## Composition Scales

> "Pulse + Baseline = Timing. But the pattern doesn't stop there.  
> Timing + Timing = System Health. Composition scales."

This final lesson explores where the timing health pattern applies and how it extends to larger systems.

---

## 1. Application: Service Health Monitor

### The Problem

A microservice calls multiple downstream APIs:
- Auth service (~50ms response)
- Database (~20ms response)
- Cache (~5ms response)
- External API (~200ms response)

Each has different "normal" timing. Static thresholds don't work.

### The Solution

One timing monitor per dependency:

```c
typedef struct {
    timing_fsm_t auth_timing;
    timing_fsm_t db_timing;
    timing_fsm_t cache_timing;
    timing_fsm_t external_timing;
} service_health_t;

void on_auth_response(service_health_t *h, uint64_t ts) {
    timing_result_t r = timing_heartbeat(&h->auth_timing, ts);
    if (r.is_anomaly) {
        log_warning("Auth service timing anomaly: z=%.2f", r.z);
    }
}
```

### Benefits

- **Adaptive per-service:** Each learns its own baseline
- **No manual tuning:** No need to pick thresholds
- **Drift detection:** Catches gradual degradation
- **Bounded resources:** O(1) memory per service

---

## 2. Application: Cardiac Rhythm Analyzer

### The Problem

A pacemaker monitors natural heartbeats. It needs to detect:
- **Bradycardia:** Heart rate too slow
- **Tachycardia:** Heart rate too fast
- **Arrhythmia:** Irregular rhythm
- **Asystole:** No heartbeat

### The Solution

The Timing module directly applies:

```c
timing_fsm_t cardiac_monitor;
timing_config_t cfg = {
    .heartbeat_timeout_ms = 2000,    /* 2 seconds = asystole */
    .init_window_ms       = 5000,    /* 5 seconds to establish rhythm */
    .alpha                = 0.2,     /* Faster adaptation for cardiac */
    .epsilon              = 1e-6,
    .k                    = 2.5,     /* More sensitive threshold */
    .n_min                = 10       /* ceil(2/0.2) = 10 */
};

void on_heartbeat(uint64_t timestamp_ms) {
    timing_result_t r = timing_heartbeat(&cardiac_monitor, timestamp_ms);
    
    switch (r.state) {
        case TIMING_HEALTHY:
            /* Normal sinus rhythm */
            break;
        case TIMING_UNHEALTHY:
            /* Arrhythmia detected */
            if (r.dt < baseline_mean * 0.7) {
                alert_tachycardia();
            } else if (r.dt > baseline_mean * 1.3) {
                alert_bradycardia();
            } else {
                alert_irregular();
            }
            break;
        case TIMING_DEAD:
            /* Asystole! */
            initiate_pacing();
            break;
    }
}
```

### Additional Constraints

For actual medical devices, you'd also need:
- Redundant monitors (TMR pattern)
- Cryptographic audit trail
- DO-178C/IEC 62304 certification
- But the core algorithm is exactly what we built

---

## 3. Application: Network Jitter Detector

### The Problem

VoIP quality depends on packet timing consistency:
- **Mean latency:** Determines delay (acceptable up to ~150ms)
- **Jitter:** Determines audio quality (must be low)

A connection with high but consistent latency sounds better than one with variable latency.

### The Solution

Feed packet inter-arrival times to Timing:

```c
timing_fsm_t jitter_monitor;

void on_packet_received(uint64_t timestamp_ms) {
    timing_result_t r = timing_heartbeat(&jitter_monitor, timestamp_ms);
    
    /* r.z is the jitter z-score */
    if (r.has_z && r.z > 2.0) {
        /* Significant jitter detected */
        metrics_increment("jitter_events");
        
        if (jitter_monitor.unhealthy_count > 5) {
            /* Sustained jitter - consider buffering or path change */
            increase_jitter_buffer();
        }
    }
}
```

### What This Detects

- **Burst jitter:** Sudden high variance
- **Network path change:** Latency baseline shifts
- **Congestion:** Gradual increase in variance
- **Packet loss:** Missing packets show as large Δt

---

## 4. Application: Industrial Timing Validation

### The Problem

A PLC expects sensor readings every 100ms:
- Readings at 95ms, 98ms, 102ms, 105ms → Normal ✓
- Readings at 50ms, 50ms, 200ms → Problem ✗

The average might be correct, but the pattern indicates an issue.

### The Solution

```c
timing_fsm_t sensor_timing;
timing_config_t cfg = {
    .heartbeat_timeout_ms = 200,   /* 2x expected interval */
    .init_window_ms       = 1000,  /* 1 second learning */
    .alpha                = 0.05,  /* Slower adaptation (stable process) */
    .k                    = 4.0,   /* Strict threshold */
    .n_min                = 40     /* Longer learning for precision */
};

/* On each sensor reading */
timing_result_t r = timing_heartbeat(&sensor_timing, reading_timestamp);

if (r.state == TIMING_UNHEALTHY) {
    /* Sensor timing anomaly - check cable, sensor, interference */
    raise_maintenance_alert();
}

if (r.state == TIMING_DEAD) {
    /* Sensor failure - activate backup */
    failover_to_backup_sensor();
}
```

---

## 5. Extension: Multi-Signal Composition

### Beyond Single Streams

What if you're monitoring multiple related signals?

```
Signal A (CPU temp) → Baseline A → deviation?
Signal B (Fan speed) → Baseline B → deviation?
Signal C (Power draw) → Baseline C → deviation?
```

A deviation in one might be normal if others also deviate (e.g., high load).

### System Health Composition

```c
typedef struct {
    base_fsm_t cpu_temp;
    base_fsm_t fan_speed;
    base_fsm_t power_draw;
    
    system_state_t state;
} system_health_t;

system_state_t compute_health(system_health_t *h) {
    int deviations = 0;
    
    if (base_state(&h->cpu_temp) == BASE_DEVIATION) deviations++;
    if (base_state(&h->fan_speed) == BASE_DEVIATION) deviations++;
    if (base_state(&h->power_draw) == BASE_DEVIATION) deviations++;
    
    /* Correlated deviations = high load (acceptable) */
    /* Single deviation = potential problem */
    
    if (deviations == 0) return SYSTEM_HEALTHY;
    if (deviations == 3) return SYSTEM_HIGH_LOAD;  /* Correlated */
    return SYSTEM_ANOMALY;  /* Uncorrelated deviation */
}
```

---

## 6. Extension: Hierarchical Health

### System of Systems

```
                    ┌─────────────────┐
                    │  Fleet Health   │
                    └────────┬────────┘
                             │
         ┌───────────────────┼───────────────────┐
         │                   │                   │
    ┌────┴────┐        ┌────┴────┐        ┌────┴────┐
    │Vehicle 1│        │Vehicle 2│        │Vehicle 3│
    └────┬────┘        └────┬────┘        └────┬────┘
         │                   │                   │
    ┌────┴────┐        ┌────┴────┐        ┌────┴────┐
    │ Engine  │        │ Engine  │        │ Engine  │
    │ Brakes  │        │ Brakes  │        │ Brakes  │
    │ Sensors │        │ Sensors │        │ Sensors │
    └─────────┘        └─────────┘        └─────────┘
```

Each level composes from the level below:

```c
/* Component level */
timing_fsm_t engine_timing;
timing_fsm_t brake_timing;
timing_fsm_t sensor_timing;

/* Vehicle level */
vehicle_state_t vehicle_health(void) {
    timing_state_t e = timing_state(&engine_timing);
    timing_state_t b = timing_state(&brake_timing);
    timing_state_t s = timing_state(&sensor_timing);
    
    if (e == TIMING_DEAD || b == TIMING_DEAD) {
        return VEHICLE_CRITICAL;
    }
    if (e == TIMING_UNHEALTHY || b == TIMING_UNHEALTHY) {
        return VEHICLE_DEGRADED;
    }
    return VEHICLE_HEALTHY;
}

/* Fleet level */
fleet_state_t fleet_health(vehicle_state_t *vehicles, int n) {
    int critical = 0;
    int degraded = 0;
    
    for (int i = 0; i < n; i++) {
        if (vehicles[i] == VEHICLE_CRITICAL) critical++;
        if (vehicles[i] == VEHICLE_DEGRADED) degraded++;
    }
    
    if (critical > 0) return FLEET_CRITICAL;
    if (degraded > n/10) return FLEET_DEGRADED;
    return FLEET_HEALTHY;
}
```

The pattern scales hierarchically.

---

## 7. Connection to Erlang/OTP Supervision Trees

Our composition pattern is similar to Erlang's supervision trees:

| Erlang/OTP | Timing Module |
|------------|---------------|
| Worker process | Component FSM (Pulse, Baseline) |
| Supervisor | Composed FSM (Timing) |
| Restart strategy | Reset/recovery logic |
| Health check | timing_check() |

The key insight: **hierarchical composition with well-defined failure modes**.

---

## 8. Where to Go From Here

### Immediate Extensions

1. **Add more signal types:** Temperature, pressure, voltage
2. **Multi-stream correlation:** Detect correlated vs uncorrelated anomalies
3. **Persistence:** Save/restore learned baselines
4. **Alerting:** Integration with monitoring systems

### Advanced Topics

1. **Formal verification:** Prove contracts with TLA+ or Coq
2. **Certification:** Prepare for DO-178C/IEC 62304
3. **Hardware implementation:** FPGA/ASIC for deterministic timing
4. **Distributed composition:** Network of monitors

### The Bigger Picture

```
Module 1: Pulse      → Existence in time
Module 2: Baseline   → Normality in value
Module 3: Timing     → Health over time
Module 4: ???        → Multi-signal correlation?
Module 5: ???        → Hierarchical health?
```

The pattern continues.

---

## 9. The Trilogy Complete

| Module | Question | Answer |
|--------|----------|--------|
| **Pulse** | Does something exist? | ALIVE / DEAD |
| **Baseline** | Is a value normal? | STABLE / DEVIATION |
| **Timing** | Is the rhythm healthy? | HEALTHY / UNHEALTHY |

Each module:
- Is closed, total, deterministic
- Has proven contracts
- Uses O(1) memory
- Composes with others

Together they form a foundation for monitoring any time-series system.

---

## Exercises

### Exercise 6.1: Design a Service Monitor

Design a complete service health monitor for a web service with:
- 3 downstream dependencies
- Different "normal" latencies for each
- Combined health state

How many timing_fsm_t instances do you need?

### Exercise 6.2: Cardiac Extensions

What additional contracts would a real cardiac monitor need?
- Response time guarantees?
- Redundancy requirements?
- Audit requirements?

### Exercise 6.3: Fleet Composition

Design the state machine for fleet-level health:
- States: HEALTHY, DEGRADED, CRITICAL, OFFLINE
- Transitions based on vehicle states
- What should the mapping function look like?

### Exercise 6.4: Erlang Comparison

Read about Erlang/OTP supervision trees. How does our composition pattern compare?
- What's similar?
- What's different?
- What could we learn from Erlang's approach?

---

## Summary

**Applications:**
- Service health monitoring
- Cardiac rhythm analysis
- Network jitter detection
- Industrial timing validation

**Extensions:**
- Multi-signal composition
- Hierarchical health
- System of systems

**The Pattern:**
> Compose proven components through well-defined interfaces.  
> The composition inherits the guarantees.

---

## Conclusion

You've now built three modules that form a complete monitoring foundation:

1. **Pulse:** Proves existence over time
2. **Baseline:** Proves normality in value
3. **Timing:** Proves health through composition

Each follows the methodology:
- Mathematical design first
- Structs as transcription
- Code as wiring
- Tests as verification

This is how safety-critical software is built: prove, transcribe, verify.

---

*"The open source outlives us."*

---

[Previous: Lesson 5 — Testing](../05-testing/LESSON.md) | [Back to README](../../README.md)
