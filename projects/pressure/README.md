# Pressure — Bounded Queue with Backpressure

**Module 6 of c-from-scratch**

> "When messages arrive faster than you can process them,
> you have three choices: drop, block, or explode.
> Only bounded queues let you choose deliberately."

## What Is This?

A closed, total, deterministic ring buffer implementation with configurable overflow policies. Provides explicit backpressure signaling through pressure states.

## Contracts (Proven)

```
CONTRACT-1 (Bounded Memory):  Queue never exceeds configured capacity
CONTRACT-2 (No Data Loss):    Every item is queued, rejected, or dropped (tracked)
CONTRACT-3 (FIFO Ordering):   Items dequeue in insertion order
CONTRACT-4 (Pressure Signal): Fill level accurately reflects queue state
```

## Properties

| Property | Guaranteed |
|----------|------------|
| Memory | O(capacity) fixed |
| Enqueue | O(1) |
| Dequeue | O(1) |
| Determinism | Yes |

## Quick Start

```bash
make
make test   # Run contract tests
make demo   # See all overflow policies
```

## Overflow Policies

| Policy | Behaviour | Use Case |
|--------|-----------|----------|
| REJECT | Return ERR_FULL | Backpressure to producer |
| DROP_OLDEST | Overwrite oldest | Real-time streams |
| DROP_NEWEST | Discard incoming | Preserve history |

## Pressure States

| State | Fill Level | Action |
|-------|------------|--------|
| LOW | < 25% | Normal operation |
| NORMAL | 25-75% | Normal operation |
| HIGH | 75-90% | Consider slowing producer |
| CRITICAL | > 90% | Shed load immediately |

## API

```c
int pressure_init(pressure_queue_t *q, const pressure_config_t *cfg,
                  pressure_item_t *buffer);

int pressure_enqueue(pressure_queue_t *q, uint64_t payload,
                     uint64_t timestamp, pressure_result_t *result);

int pressure_dequeue(pressure_queue_t *q, pressure_item_t *item,
                     pressure_result_t *result);

void pressure_reset(pressure_queue_t *q);
```

## Test Results

```
CONTRACT-1: Bounded memory
CONTRACT-2: No data loss (accounting)
CONTRACT-3: FIFO ordering
CONTRACT-4: Pressure signal accuracy

Policy: REJECT, DROP_OLDEST, DROP_NEWEST

Fuzz: 100000 random ops, invariants held

Results: 16/16 tests passed
```

## License

MIT License - See LICENSE in repository root.

## Author

William Murray  
Copyright (c) 2026
