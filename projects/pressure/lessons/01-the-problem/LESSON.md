# c-from-scratch — Module 6: Pressure

## Lesson 1: The Problem

---

## The Overflow Crisis

> "When messages arrive faster than you can process them,
> you have three choices: drop, block, or explode.
> Only bounded queues let you choose deliberately."

Every safety-critical system must handle the moment when input rate exceeds processing rate. The question isn't *if* it will happen, but *what happens when it does*.

---

## Real-World Failures

### Toyota Unintended Acceleration (2009-2011)

NASA's investigation found unbounded task queues in the engine control software. Under high load, memory exhaustion caused undefined behaviour.

### Knight Capital (2012)

A trading system processed 4 million orders in 45 minutes due to a software bug. No backpressure. $440 million lost.

### Amazon DynamoDB Outage (2015)

Metadata service overwhelmed by requests. No rate limiting. Cascading failure across multiple AWS services.

### Cloudflare Outage (2019)

A single regex rule caused CPU exhaustion. No circuit breaker. Global outage for 27 minutes.

**The pattern:** Unbounded queues + no backpressure = catastrophe.

---

## Why Unbounded Queues Fail

```c
// The deadly pattern
while (1) {
    msg = receive();
    queue_push(queue, msg);  // What if queue grows forever?
}
```

| Problem | Consequence |
|---------|-------------|
| Memory exhaustion | OOM killer terminates process |
| Latency explosion | Items wait hours to be processed |
| Priority inversion | Old stale items block fresh ones |
| No backpressure | Producer never knows to slow down |

---

## The Three Choices

When a bounded queue is full and a new item arrives:

### 1. REJECT (Block Producer)

```
Producer: "Here's a message"
Queue:    "I'm full. ERR_FULL."
Producer: "I'll slow down or retry."
```

**Use case:** When every message matters and producer can handle backpressure.

### 2. DROP_OLDEST (Lossy, Never Blocks)

```
Producer: "Here's a message"
Queue:    "Overwriting oldest item to make room."
Producer: "OK, I'll keep going."
```

**Use case:** Telemetry, metrics, real-time streams where freshness matters more than completeness.

### 3. DROP_NEWEST (Preserve History)

```
Producer: "Here's a message"
Queue:    "Discarding your message. Queue unchanged."
Producer: "OK, I'll keep going."
```

**Use case:** Audit logs, transaction history where older events are more valuable.

---

## What Makes a Good Queue?

| Property | Meaning |
|----------|---------|
| **Bounded** | Fixed maximum size, never grows |
| **Accountable** | Every item tracked: queued, dropped, or rejected |
| **Observable** | Fill level visible for backpressure signals |
| **Deterministic** | Same inputs → Same behaviour |
| **O(1)** | Constant-time enqueue and dequeue |

---

## The Contracts We'll Prove

```
CONTRACT-1 (Bounded Memory):  Queue never exceeds configured capacity
CONTRACT-2 (No Data Loss):    Every item is queued, rejected, or dropped (tracked)
CONTRACT-3 (FIFO Ordering):   Items dequeue in insertion order
CONTRACT-4 (Pressure Signal): Fill level accurately reflects queue state
```

---

## The Key Insight: Ring Buffers

A ring buffer is a fixed-size array with two pointers:

```
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ A │ B │ C │ D │   │   │   │   │
└───┴───┴───┴───┴───┴───┴───┴───┘
  ↑               ↑
 head            tail
 (read)          (write)
```

- **head**: Where we read (dequeue) from
- **tail**: Where we write (enqueue) to
- **count**: Number of items currently stored

When tail reaches the end, it wraps to the beginning:

```
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ G │ H │   │   │ E │ F │   │   │
└───┴───┴───┴───┴───┴───┴───┴───┘
          ↑       ↑
         tail    head
```

**O(1) operations, O(capacity) memory, always bounded.**

---

## From Module 5 to Module 6

| Module | Question | Output |
|--------|----------|--------|
| Pulse | Does it exist? | ALIVE / DEAD |
| Baseline | Is it normal? | STABLE / DEVIATION |
| Timing | Is it healthy? | HEALTHY / UNHEALTHY |
| Drift | Moving toward failure? | STABLE / DRIFTING |
| Consensus | Which sensor to trust? | Voted value |
| **Pressure** | **How to handle overflow?** | **Bounded queue + backpressure** |

---

## Lesson 1 Checklist

- ☐ Understand why unbounded queues cause failures
- ☐ Know the three overflow policies
- ☐ Understand ring buffer mechanics
- ☐ State the four contracts

---

## Key Takeaway

> **Module 5 proved truth from many liars.**
> **Module 6 proves graceful degradation under pressure.**

In the next lesson, we formalise the ring buffer mathematically.
