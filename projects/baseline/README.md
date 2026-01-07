# Baseline - Statistical Normality Monitor

**Module 2 of c-from-scratch**

> Module 1 proved existence in time.
> Module 2 proves normality in value.

## What Is This?

Baseline is a closed, total, deterministic finite state machine for detecting statistical anomalies in scalar observation streams.

Given a stream of values (CPU usage, latency, heartbeat intervals), Baseline answers:

> "Is this value normal relative to what we've seen before?"

## Contracts (Proven)

```
CONTRACT-1 (Convergence):    μₜ → E[X] for stationary input
CONTRACT-2 (Sensitivity):    Deviations detected in O(1/α) steps
CONTRACT-3 (Stability):      False positive rate bounded by P(|Z|>k)
CONTRACT-4 (Spike Resistance): |Δμ| ≤ α·M for single outlier M
```

## Properties

| Property | Guaranteed |
|----------|------------|
| Memory | O(1) - constant |
| Time per step | O(1) - constant |
| Determinism | Yes |
| Closure | Yes |
| Recoverability | Yes |

## Quick Start

```bash
# Build everything
make

# Run the demo
make demo

# Run contract tests
make test
```

## Project Structure

```
baseline/
├── include/
│   └── baseline.h      # API and contracts
├── src/
│   ├── baseline.c      # Implementation
│   └── main.c          # Demo
├── tests/
│   └── test_baseline.c # Contract test suite
├── lessons/
│   ├── 01-the-problem/
│   ├── 02-mathematical-model/
│   ├── 03-structs/
│   ├── 04-code/
│   ├── 05-testing/
│   └── 06-composition/
├── build/              # Compiled artifacts
├── Makefile
└── README.md
```

## Lessons

1. **The Problem** - Why naive anomaly detection fails
2. **Mathematical Model** - EMA, z-scores, and the FSM
3. **Structs** - Encoding state without losing meaning
4. **Code** - Mathematical transcription in C
5. **Testing** - Contracts as theorems, tests as proofs
6. **Composition** - Pulse + Baseline pipeline

## API

```c
// Initialize
int base_init(base_fsm_t *b, const base_config_t *cfg);

// Process one observation (atomic step)
base_result_t base_step(base_fsm_t *b, double x);

// Reset to LEARNING state
void base_reset(base_fsm_t *b);

// Query functions
base_state_t base_state(const base_fsm_t *b);
uint8_t base_faulted(const base_fsm_t *b);
uint8_t base_ready(const base_fsm_t *b);
```

## Composition with Pulse

```
event_t → Pulse → Δt → Baseline → deviation?
```

Pulse outputs inter-arrival times. Baseline monitors them.
Result: timing anomaly detection without thresholds.

## Test Results

```
Contract Tests:
  [PASS] CONTRACT-1: Convergence
  [PASS] CONTRACT-2: Sensitivity
  [PASS] CONTRACT-3: Stability
  [PASS] CONTRACT-4: Spike Resistance

Invariant Tests:
  [PASS] INV-1: State domain
  [PASS] INV-2: Ready implies evidence
  [PASS] INV-3: Fault implies DEVIATION
  [PASS] INV-5: Variance non-negative
  [PASS] INV-7: Monotonic count

Results: 18/18 tests passed
```

## License

MIT License - See LICENSE in repository root.

## Author

William Murray  
Copyright (c) 2025
