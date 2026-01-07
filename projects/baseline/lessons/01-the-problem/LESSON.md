# c-from-scratch — Module 2: Baseline

## Lesson 1: The Problem

---

## Why Anomaly Detection Matters

Every significant system failure shares a pattern: something changed, monitoring said "fine", humans discovered the problem too late.

### Real-World Failures

**AWS S3 Outage (2017)**

A subsystem's metrics looked "normal" by static thresholds. Reality: "normal" had slowly drifted over weeks. Nobody noticed until cascading failure brought down half the internet.

Cost: 4 hours of internet disruption.

**Knight Capital (2012)**

Trading volume was "within thresholds". But the *pattern* was completely abnormal — the system was executing a decade of trades in minutes. Static rules couldn't see it.

Cost: $440 million in 45 minutes.

**Cloudflare Outage (2019)**

CPU usage was "acceptable" by threshold. But the *rate of change* was pathological — usage was climbing faster than any normal workload. Monitoring said green, reality said fire.

Cost: 30 minutes of global outage.

### The Pattern

Every time:

1. Something changes (gradually or suddenly)
2. Static thresholds say "fine"
3. Humans discover the problem too late
4. Expensive consequences

The question isn't whether your system will drift. The question is whether you'll notice before it fails.

---

## Why Naive Approaches Fail

### Simple Moving Average (SMA)

```c
// "Just average the last N values"
double sma = sum(buffer) / N;
```

| Problem | Consequence |
|---------|-------------|
| O(N) memory | Buffer grows with window size |
| O(N) update | Must sum entire buffer each time |
| Spike corruption | One outlier poisons N future comparisons |
| Not closed | State depends on unbounded history |

The SMA looks simple, but it's fundamentally broken for real systems. When a spike enters the window, it corrupts your baseline for the next N observations. There's no bound on how badly a single bad value can hurt you.

**Formal problem:** SMA is not a valid state machine. It violates closure.

### Static Thresholds

```c
// "Alert if CPU > 80%"
if (cpu > 80.0) alert();
```

| Problem | Consequence |
|---------|-------------|
| No context | 80% might be normal on batch day |
| No adaptation | System changes, thresholds don't |
| Binary | No "how abnormal?" quantification |
| Manual tuning | Requires constant adjustment |

Static thresholds encode yesterday's assumptions about today's system. They can't adapt, they can't learn, and they produce either floods of false positives or dangerous false negatives.

### Unbounded History

```c
// "Store everything, compute stats on demand"
observations.push_back(x);
double mean = compute_mean(observations);  // O(N)!
```

| Problem | Consequence |
|---------|-------------|
| Memory grows forever | Eventually OOM |
| Compute grows forever | Eventually too slow |
| Old data pollutes | Ancient history affects current baseline |
| Not deterministic | Behaviour depends on history length |

This is the "data science" approach: just store everything and compute statistics when needed. It works in notebooks. It fails in production. Your monitoring system shouldn't be the thing that runs out of memory.

---

## What We Actually Need

A system that is:

| Property | Meaning |
|----------|---------|
| **Closed** | State depends only on previous state + new input |
| **Bounded** | O(1) memory, O(1) update |
| **Adaptive** | Learns what "normal" means |
| **Recoverable** | Single spike doesn't corrupt baseline |
| **Quantified** | "How abnormal?" not just "abnormal?" |
| **Deterministic** | Same inputs → same outputs |

This isn't a wishlist. These are requirements. Any system that violates them will eventually fail in production.

### The Contracts We'll Prove

```
CONTRACT-1 (Convergence):    Baseline converges to true mean
CONTRACT-2 (Sensitivity):    Deviations detected in O(1/α) steps  
CONTRACT-3 (Stability):      False positive rate bounded
CONTRACT-4 (Spike Resistance): Single outlier shifts mean by at most α·M
```

These aren't aspirations. They're theorems we'll prove in code.

---

## The EMA Insight

**Exponential Moving Average** gives us everything:

```
μₜ = α·xₜ + (1 - α)·μₜ₋₁
```

One line of mathematics. Every property we need:

| Property | How EMA Delivers |
|----------|------------------|
| Closed | μₜ depends only on μₜ₋₁ and xₜ |
| Bounded | O(1) memory: just store μ and σ² |
| Adaptive | Recent observations weighted more |
| Recoverable | Spike shifts mean by at most α·M |
| Quantified | z-score: \|x - μ\| / σ |
| Deterministic | Pure function of inputs |

**This is not an optimisation. This is what makes the system closed.**

The SMA requires a buffer. The EMA requires a single number. The SMA can be corrupted for N steps. The EMA's corruption is bounded by α. The SMA grows with your window. The EMA is O(1) forever.

### The Critical Insight: Spike Resistance

When a spike of magnitude M hits your baseline:

```
SMA:  Mean can shift by M/N... but N values are now poisoned
EMA:  Mean shifts by at most α·M... and that's it
```

With α = 0.1 and a spike of 1000:

- EMA shift: at most 100 (bounded, recoverable)
- SMA shift: 1000/N now, but corrupted for N more observations

CONTRACT-4 (Spike Resistance) is why EMA wins. One bad sensor reading, one network glitch, one corrupted value — and the SMA-based system needs N observations to recover. The EMA-based system recovers immediately.

---

## The Zero-Variance Problem

There's one place where pure mathematics fails: division by zero.

```
z = |x - μ| / σ
```

In pure mathematics, σ = 0 is allowed. In C, it's undefined behaviour.

This isn't a bug to work around. This is a **physical constraint of finite machines**. Any real implementation must handle it explicitly:

```c
if (variance <= epsilon) {
    z = 0.0;  // Cannot compute meaningful z-score
} else {
    z = fabs(x - mu) / sigma;
}
```

This is the first lesson in systems programming: **the machine is not the mathematics**. Every floating-point operation can fail. Every division can divide by zero. Every computation can overflow. Real systems handle these cases explicitly.

---

## From "Exists" to "Normal"

Recall the philosophical shift from Module 1:

| Module | Question | Output |
|--------|----------|--------|
| Pulse | Does something happen? | Yes / No |
| Baseline | Is what's happening normal? | Degree of deviation |

Pulse tracked events in time.
Baseline tracks values in space.

So instead of counting heartbeats, we now observe a stream:

```
x₁, x₂, x₃, …, xₜ
```

Each xₜ is a real-valued measurement: CPU usage, latency, inter-arrival time from Pulse, temperature, price — any scalar signal.

Our job: decide whether xₜ is consistent with recent history.

---

## Deliverables

By the end of Lesson 1, you understand:

- ☐ Why static thresholds fail (no adaptation, no context)
- ☐ Why unbounded history fails (O(N) memory, O(N) compute)
- ☐ Why SMA fails (spike corruption, not closed)
- ☐ The EMA insight (closed, bounded, recoverable)
- ☐ The four contracts we'll prove
- ☐ The philosophical shift from "exists" to "normal"

---

## Key Takeaway

> **Module 1 proved existence in time.**
> **Module 2 proves normality in value.**

Pulse tells us the heartbeat exists. Baseline tells us if the heart rate is pathological. Together, they compose into something more powerful: a timing anomaly detector that can tell you not just "is it alive?" but "is it healthy?"

This is the c-from-scratch philosophy: modules are not demos. They are systems. And systems compose.
