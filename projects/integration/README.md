# c-from-scratch Integration Example

## Complete Safety Monitoring System

This example demonstrates all 6 c-from-scratch modules working together in a realistic safety-critical monitoring scenario.

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        SAFETY MONITOR                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐               │
│  │  SENSOR 0    │  │  SENSOR 1    │  │  SENSOR 2    │               │
│  │              │  │              │  │  (faulty)    │               │
│  │ ┌──────────┐ │  │ ┌──────────┐ │  │ ┌──────────┐ │               │
│  │ │  Pulse   │ │  │ │  Pulse   │ │  │ │  Pulse   │ │  Module 1    │
│  │ │ (alive?) │ │  │ │ (alive?) │ │  │ │ (alive?) │ │               │
│  │ └────┬─────┘ │  │ └────┬─────┘ │  │ └────┬─────┘ │               │
│  │      ↓       │  │      ↓       │  │      ↓       │               │
│  │ ┌──────────┐ │  │ ┌──────────┐ │  │ ┌──────────┐ │               │
│  │ │ Baseline │ │  │ │ Baseline │ │  │ │ Baseline │ │  Module 2    │
│  │ │(normal?) │ │  │ │(normal?) │ │  │ │(normal?) │ │               │
│  │ └────┬─────┘ │  │ └────┬─────┘ │  │ └────┬─────┘ │               │
│  │      ↓       │  │      ↓       │  │      ↓       │               │
│  │ ┌──────────┐ │  │ ┌──────────┐ │  │ ┌──────────┐ │               │
│  │ │  Timing  │ │  │ │  Timing  │ │  │ │  Timing  │ │  Module 3    │
│  │ │(regular?)│ │  │ │(regular?)│ │  │ │(regular?)│ │               │
│  │ └────┬─────┘ │  │ └────┬─────┘ │  │ └────┬─────┘ │               │
│  │      ↓       │  │      ↓       │  │      ↓       │               │
│  │ ┌──────────┐ │  │ ┌──────────┐ │  │ ┌──────────┐ │               │
│  │ │  Drift   │ │  │ │  Drift   │ │  │ │  Drift   │ │  Module 4    │
│  │ │(trend?)  │ │  │ │(trend?)  │ │  │ │(trend?)  │ │               │
│  │ └────┬─────┘ │  │ └────┬─────┘ │  │ └────┬─────┘ │               │
│  │      ↓       │  │      ↓       │  │      ↓       │               │
│  │   HEALTH    │  │   HEALTH     │  │   HEALTH     │               │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘               │
│         │                 │                 │                        │
│         └────────────────┬┴─────────────────┘                        │
│                          ↓                                           │
│                  ┌──────────────┐                                    │
│                  │  CONSENSUS   │  Module 5                          │
│                  │   (TMR Vote) │                                    │
│                  └──────┬───────┘                                    │
│                         ↓                                            │
│                  ┌──────────────┐                                    │
│                  │   PRESSURE   │  Module 6                          │
│                  │   (Queue)    │                                    │
│                  └──────┬───────┘                                    │
│                         ↓                                            │
│                     OUTPUT                                           │
└─────────────────────────────────────────────────────────────────────┘
```

## Scenario

1. **Three redundant sensors** monitor a value (ground truth = 100.0)
2. **Sensor 2 starts drifting** at tick 40 (rate = 0.3/tick)
3. **Sensor 2 fails completely** at tick 70
4. **Consensus voting** maintains accuracy despite failures
5. **Output queue** buffers results with backpressure management

## Module Roles

| Module | Question | Role in Pipeline |
|--------|----------|------------------|
| Pulse | Is sensor alive? | Detect dead sensors |
| Baseline | Is value normal? | Detect outliers |
| Timing | Is timing regular? | Detect jitter |
| Drift | Is value trending? | Detect slow failures |
| Consensus | Which sensor to trust? | TMR voting |
| Pressure | Handle overflow? | Output buffering |

## Build & Run

```bash
make
make run
```

## Expected Output

The system should:
- Detect sensor 2 drifting (DEGRADED health)
- Detect sensor 2 failure (FAULTY health)
- Maintain consensus accuracy within 1% of ground truth
- Buffer all outputs gracefully

## Key Insights

1. **Defense in depth**: Four modules monitor each sensor
2. **Graceful degradation**: System continues with 2 healthy sensors
3. **Full visibility**: Health states propagate to consensus
4. **Bounded resources**: Output queue never overflows

## License

MIT License - Copyright (c) 2026 William Murray
