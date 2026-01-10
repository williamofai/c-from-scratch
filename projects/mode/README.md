# Mode Manager — The Captain

**Module 7 of c-from-scratch**

> "While Modules 1-6 answer 'What's happening?',
> Module 7 answers 'What do we DO about it?'"

## What Is This?

A closed, total, deterministic state machine that orchestrates system behaviour based on health signals from all foundation modules. The Mode Manager is the "Captain" of the safety-critical ship.

## Contracts (Proven)

```
CONTRACT-1: Unambiguous state   — System in exactly one mode at any time
CONTRACT-2: Safe entry          — OPERATIONAL requires all monitors healthy
CONTRACT-3: Fault stickiness    — EMERGENCY requires explicit reset
CONTRACT-4: No skip             — Must pass through STARTUP to reach OPERATIONAL
CONTRACT-5: Bounded latency     — Fault → EMERGENCY in ≤ 1 cycle
CONTRACT-6: Deterministic       — Same inputs → Same mode
CONTRACT-7: Proactive safety    — Value flags trigger DEGRADED before faults
CONTRACT-8: Auditability        — All transitions logged with cause
```

## System Modes

| Mode | Meaning | Actions Allowed |
|------|---------|-----------------|
| INIT | Power-on, validating | Log, Communicate |
| STARTUP | Learning period | + Calibrate |
| OPERATIONAL | Normal operation | + Actuate |
| DEGRADED | Approaching limits | Log, Communicate |
| EMERGENCY | Critical fault (sticky) | Log, Communicate |
| TEST | Maintenance override | Full access |

## Quick Start

```bash
make
make test   # 17/17 contract tests
make demo   # See mode transitions
```

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                    MODULE 7: MODE MANAGER                           │
│                                                                     │
│   Inputs:                                                           │
│     - states[6]  (health from each foundation module)               │
│     - flags      (approaching-limit warnings)                       │
│                                                                     │
│   Outputs:                                                          │
│     - mode       (current system mode)                              │
│     - permissions (what actions are allowed)                        │
│     - history    (audit log of transitions)                         │
└─────────────────────────────────────────────────────────────────────┘
```

## Value-Aware Safety

The Mode Manager doesn't just react to faults — it anticipates them:

| Approach | Behaviour |
|----------|-----------|
| State-only | "The sensor failed" (reactive) |
| Value-aware | "The sensor is ABOUT to fail" (proactive) |

Semantic flags enable proactive degradation:
- `approaching_upper` — Drift TTF below threshold
- `low_confidence` — Consensus confidence below 50%
- `queue_critical` — Pressure queue above 90%

## Test Results

```
CONTRACT-1: Unambiguous state (mode always valid)
CONTRACT-2: Safe entry (OPERATIONAL requires all healthy)
CONTRACT-3: Fault stickiness (EMERGENCY requires reset)
CONTRACT-4: No skip (must pass through STARTUP)
CONTRACT-5: Bounded latency (fault → EMERGENCY in 1 cycle)
CONTRACT-6: Deterministic (same inputs → same mode)
CONTRACT-7: Proactive safety (flags trigger DEGRADED)
CONTRACT-8: Auditability (all transitions logged)

Fuzz: 10000 random inputs, no crashes

Results: 17/17 tests passed
```

## API

```c
int mode_init(mode_manager_t *m, const mode_config_t *cfg);
int mode_update(mode_manager_t *m, const mode_input_t *input, mode_result_t *result);
void mode_reset(mode_manager_t *m);

/* Queries */
system_mode_t mode_get(const mode_manager_t *m);
int mode_can_actuate(const mode_manager_t *m);
int mode_is_fault(const mode_manager_t *m);
```

## The Complete Stack

| Module | Question | Role |
|--------|----------|------|
| Pulse | Does it exist? | Sensor |
| Baseline | Is it normal? | Sensor |
| Timing | Is it regular? | Sensor |
| Drift | Is it trending? | Sensor |
| Consensus | Which to trust? | Judge |
| Pressure | Handle overflow? | Buffer |
| **Mode** | **What to do?** | **Captain** |

## License

MIT License - Copyright (c) 2026 William Murray
