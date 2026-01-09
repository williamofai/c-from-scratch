# Lesson 1: The Problem

## Beyond Existence and Normality

> "Pulse tells us something exists. Baseline tells us if values are normal.  
> But neither alone tells us if the rhythm is healthy."

In Module 1, we built Pulse—a heartbeat liveness monitor that answers: *"Is this process alive?"*

In Module 2, we built Baseline—a statistical normality monitor that answers: *"Is this value normal?"*

Now we compose them to answer a deeper question: *"Is the timing healthy?"*

---

## Why Timing Health Matters

### The Lambda Cold Start Problem

AWS Lambda functions have unpredictable startup times. A function that usually responds in 50ms might take 3 seconds on a cold start.

**The naive approach:** Set a static timeout threshold.

```
if (response_time > 1000ms) {
    alert("slow!");
}
```

**The problem:**
- Threshold too low → constant false alarms on cold starts
- Threshold too high → miss genuine slowdowns

What you actually need is a system that learns "normal" response patterns and adapts.

### Cardiac Pacemakers

A pacemaker must detect when natural heartbeats are:
- Too slow (bradycardia)
- Too fast (tachycardia)
- Irregular (arrhythmia)

The detection must be:
- **Fast enough** to intervene before the patient suffers
- **Accurate enough** to avoid unnecessary pacing
- **Robust** to noise and artifacts

This is timing anomaly detection where lives depend on correctness.

### Network Jitter in VoIP

Voice calls degrade when packet timing becomes irregular. It's not about average latency—it's about *variance*.

- A call with consistent 100ms latency sounds fine
- A call alternating between 20ms and 180ms sounds terrible

The average might be identical, but the experience is completely different.

### Industrial Control Systems

A PLC expects sensor readings every 100ms:
- 95ms, 98ms, 102ms, 105ms → Normal jitter ✓
- 50ms, 50ms, 200ms → Something is wrong ✗

The average is still ~100ms, but the pattern indicates a problem.

**Key insight:** Pattern matters more than average.

---

## What Existing Tools Get Wrong

### Static Thresholds Don't Adapt

```python
# The typical approach
TIMEOUT = 5000  # milliseconds

if time_since_last_heartbeat > TIMEOUT:
    declare_dead()
```

Problems:
- What if "normal" is 4500ms? You'll get constant near-misses.
- What if "normal" is 100ms? 5000ms is way too late to detect failure.
- What if normal timing drifts over time?

### Monitoring Existence Isn't Enough

Pulse can tell us:
- Process is ALIVE (heartbeat within timeout)
- Process is DEAD (timeout expired)

But Pulse can't tell us:
- Heartbeats are arriving erratically (jitter)
- Heartbeats are gradually slowing down (drift)
- Heartbeats come in bursts (stalls)

A process can be "alive" but "unhealthy."

### Monitoring Values Isn't Enough

Baseline can tell us:
- This value is within normal range
- This value is a statistical outlier

But Baseline doesn't know about timing:
- It needs someone to feed it observations
- It doesn't know *when* observations should arrive
- It can't detect missing observations

### The Missing Piece

What we need is the *composition* of both:

```
event → Pulse → Δt → Baseline → timing_anomaly?
```

Pulse converts events into inter-arrival times (Δt).
Baseline monitors those times for statistical anomalies.
The composition detects timing health.

---

## What We're Building

A composed timing health monitor that:

| Property | Meaning |
|----------|---------|
| **Adaptive** | Learns what "normal" timing looks like |
| **Sensitive** | Detects jitter, drift, and stalls |
| **Specific** | Low false positive rate |
| **Composable** | Built from proven modules |
| **Deterministic** | Same inputs → same outputs |
| **Bounded** | O(1) memory, O(1) compute |

### The Four States

```
INITIALIZING  Neither module has sufficient evidence
HEALTHY       Pulse is ALIVE and Baseline is STABLE
UNHEALTHY     Pulse is ALIVE but timing is anomalous
DEAD          Pulse is DEAD (no heartbeats)
```

### The Guarantees

The composed system inherits guarantees from both components:

1. **From Pulse:** Eventually detect death (liveness)
2. **From Pulse:** Never claim alive if dead (soundness)
3. **From Baseline:** Detect sustained anomalies (sensitivity)
4. **From Baseline:** Resist single spikes (robustness)
5. **From Both:** Deterministic composition (reproducibility)

---

## Why Composition?

Most monitoring systems are monolithic—one big blob of code that "does monitoring." When something goes wrong, you debug the blob.

Compositional design is different:
- Each module has **proven contracts**
- Modules compose through **well-defined interfaces**
- The composition **inherits the guarantees**
- Debugging is **modular**: which contract broke?

### The Composition Theorem

If Module A is closed, total, and deterministic, and Module B is closed, total, and deterministic, then (A → B) is closed, total, and deterministic.

**Proof sketch:**
- Closed: State depends only on previous state + input (both modules)
- Total: Every input produces valid output (both modules)
- Deterministic: Same inputs → same outputs (both modules)

The composition preserves these properties because it's just wiring.

---

## Exercises

### Exercise 1.1: Lambda Cold Starts

Design a static threshold for AWS Lambda cold starts:
- Normal response: 50ms
- Cold start response: 2500ms
- Cold start frequency: ~1% of requests

What threshold would you set? What goes wrong?

### Exercise 1.2: Pacemaker Analysis

A pacemaker sees heartbeats at these intervals:
- 800ms, 820ms, 1200ms, 810ms, 790ms

Is this healthy? How would you detect the anomaly at 1200ms without flagging the normal variation (800±30ms)?

### Exercise 1.3: Jitter vs Mean

You have two servers:
- Server A: response times [100, 100, 100, 100, 100]
- Server B: response times [50, 150, 50, 150, 50]

Both have mean = 100ms. Which is healthier? Why can't you detect the difference with just a mean?

### Exercise 1.4: Static vs Adaptive

A service normally responds in ~500ms. Due to increased load, response times gradually drift to ~800ms over an hour.

- How would a static threshold (e.g., 1000ms) handle this?
- How would an adaptive baseline handle this?
- Which is better for detecting the drift?

---

## Summary

**The Problem:**
> Existing tools monitor existence OR values, but not timing health.

**Our Solution:**
> Compose Pulse (existence) and Baseline (normality) into a timing health monitor.

**The Method:**
> Feed inter-arrival times from Pulse into Baseline. Map combined states to health.

---

## Next Lesson

In **Lesson 2: Mathematical Model**, we'll:
- Define the composed state formally
- Build the state mapping table
- Prove that composition preserves guarantees
- Design the transition diagram

---

*"The composition inherits the contracts. Prove the components, compose them, trust the result."*

---

[Next: Lesson 2 — Mathematical Model →](../02-mathematical-model/LESSON.md)
