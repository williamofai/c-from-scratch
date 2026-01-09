# Timing — Composed Timing Health Monitor

**Module 3 of c-from-scratch**

> Module 1 proved existence. Module 2 proved normality.  
> Module 3 proves health over time.

## What Is This?

A timing anomaly detector built by composing Pulse (heartbeat liveness) and Baseline (statistical normality). Detects jitter, drift, stalls, and arrhythmia without manual threshold tuning.

## The Composition

```
event → Pulse → Δt → Baseline → timing_anomaly?
```

Pulse emits inter-arrival times (Δt). Baseline monitors those times for statistical anomalies. The composition detects:

- **Jitter** — Erratic timing variations
- **Drift** — Gradual slowdown or speedup  
- **Stalls** — Missing heartbeats followed by bursts
- **Arrhythmia** — Irregular patterns

## States

| State | Meaning |
|-------|---------|
| `INITIALIZING` | Learning phase — insufficient evidence |
| `HEALTHY` | Normal rhythm — pulse alive, timing stable |
| `UNHEALTHY` | Timing anomaly — pulse alive, timing deviated |
| `DEAD` | No heartbeat — pulse dead |

## Contracts (Proven)

| Contract | Guarantee |
|----------|-----------|
| **Existence Inheritance** | Dead pulse → Dead timing |
| **Normality Inheritance** | Timing deviation → Unhealthy |
| **Health Requires Evidence** | No premature health claims |
| **Bounded Detection** | Anomalies detected in O(1/α) steps |
| **Spike Resistance** | Single anomaly can't corrupt baseline |
| **Deterministic** | Same inputs → Same outputs |

## Properties

| Property | Guaranteed |
|----------|------------|
| Memory | O(1) ~200 bytes |
| Time | O(1) per heartbeat |
| Determinism | Yes |
| Closure | Yes |

## Quick Start

```bash
make
make demo
make test
```

## Project Structure

```
timing/
├── include/
│   └── timing.h              # API + contracts
├── src/
│   ├── timing.c              # Composition implementation
│   └── main.c                # Demo program
├── tests/
│   └── test_timing.c         # Contract + fuzz tests
├── lessons/
│   ├── 01-the-problem/
│   ├── 02-mathematical-model/
│   ├── 03-structs/
│   ├── 04-code/
│   ├── 05-testing/
│   └── 06-applications/
├── Makefile
└── README.md
```

## Dependencies

Module 3 depends on Modules 1 and 2:

- `../pulse/` — Heartbeat Liveness Monitor
- `../baseline/` — Statistical Normality Monitor

## API

```c
/* Initialize timing monitor */
int timing_init(timing_fsm_t *t, const timing_config_t *cfg);

/* Process heartbeat event */
timing_result_t timing_heartbeat(timing_fsm_t *t, uint64_t timestamp_ms);

/* Check for timeout (call periodically) */
timing_result_t timing_check(timing_fsm_t *t, uint64_t current_time_ms);

/* Reset to initial state */
void timing_reset(timing_fsm_t *t);

/* Query functions */
timing_state_t timing_state(const timing_fsm_t *t);
uint8_t timing_faulted(const timing_fsm_t *t);
uint8_t timing_healthy(const timing_fsm_t *t);
const char* timing_state_name(timing_state_t st);
```

## Usage Example

```c
#include "timing.h"

timing_fsm_t monitor;
timing_config_t cfg = TIMING_DEFAULT_CONFIG;

timing_init(&monitor, &cfg);

/* On each heartbeat */
timing_result_t r = timing_heartbeat(&monitor, current_time_ms());

switch (r.state) {
    case TIMING_INITIALIZING:
        /* Still learning normal timing patterns */
        break;
    case TIMING_HEALTHY:
        /* Normal rhythm */
        break;
    case TIMING_UNHEALTHY:
        /* Timing anomaly detected! */
        printf("Anomaly: z-score = %.2f\n", r.z);
        break;
    case TIMING_DEAD:
        /* Heartbeats stopped */
        handle_death();
        break;
}
```

## Test Results

```
--- Contract Tests ---
[PASS] test_contract1_existence_inheritance: Dead pulse → Dead timing
[PASS] test_contract2_normality_inheritance: Timing deviation → Unhealthy
[PASS] test_contract3_health_requires_evidence: Health requires evidence
[PASS] test_contract4_bounded_detection: Bounded detection latency
[PASS] test_contract5_spike_resistance: Spike resistance verified
[PASS] test_contract6_deterministic: Identical inputs → identical outputs

--- Integration Tests ---
[PASS] test_integration_normal_rhythm: Normal rhythm stays healthy
[PASS] test_integration_recovery: Recovers from temporary anomaly
[PASS] test_integration_reset: Reset clears state

--- Fuzz Tests ---
[PASS] test_fuzz_random_timestamps: 10000 random timestamps
[PASS] test_fuzz_edge_timestamps: Edge cases handled

Results: 12/12 tests passed
```

## Applications

- **Service Health Monitoring** — Detect Lambda cold starts, API latency anomalies
- **Cardiac Rhythm Analysis** — Pacemaker timing, arrhythmia detection
- **Network Jitter Detection** — VoIP quality, packet timing analysis
- **Industrial Control** — PLC sensor timing, control loop monitoring

## The Trilogy

| Module | Question | Input | Output |
|--------|----------|-------|--------|
| **Pulse** | Does it exist? | Timestamps | Alive/Dead + Δt |
| **Baseline** | Is it normal? | Scalars | Normal/Deviation + z |
| **Timing** | Is it healthy? | Events + Time | Healthy/Unhealthy |

## License

MIT License — See [LICENSE](../../LICENSE)

## Author

William Murray — [@williamofai](https://github.com/williamofai)
