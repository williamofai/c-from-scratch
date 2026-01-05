# Lesson 1: The Problem

## C-From-Scratch: Build a Real Safety Microkernel

---

## What We're Building

A **heartbeat-based liveness monitor** — a tiny program that answers one question:

> **"Is this process still alive?"**

And answers it **correctly**, **always**, under **all conditions**.

That's it. One question. But answered with mathematical certainty.

---

## Why This Matters

Every production system needs liveness monitoring. When a critical service dies at 3 AM, you need to know. Not "probably know" — **know**.

Here's what happens when liveness monitoring fails:

### Real-World Failures

**Knight Capital (2012)** — $440 million lost in 45 minutes
- A dormant process was accidentally activated
- No system detected the anomalous behaviour
- By the time humans noticed, nearly half a billion dollars had evaporated

**Amazon S3 Outage (2017)** — Internet "broke" for 4 hours
- A subsystem entered a degraded state
- Health checks said "green" while reality said "broken"
- Cascading failures across thousands of dependent services

**Cloudflare Outage (2019)** — Global edge network down
- A process started consuming excessive CPU
- Monitoring dashboards showed normal... until they didn't
- 30 minutes of global outage

### The Pattern

In every case:
1. Something failed (or misbehaved)
2. The monitoring system either missed it or lied about it
3. Humans found out too late
4. Expensive consequences followed

---

## The Fundamental Question

When your monitoring system says a process is **ALIVE**, how do you know it's telling the truth?

Most monitoring tools operate on hope:

```
"We checked 5 seconds ago and it was fine,
 so it's probably still fine now."
```

**"Probably"** is not good enough for systems that matter.

---

## What Exists Today (And Why It's Not Enough)

### systemd

```bash
# Looks simple enough
[Service]
Type=notify
WatchdogSec=30
```

**Problems:**
- 1.4 million lines of code
- Complex state machine with dozens of states
- Behaviour varies by configuration, version, and distro
- When it fails, good luck debugging why
- Can you audit it? Can you *prove* it's correct?

### monit

```
check process myapp with pidfile /var/run/myapp.pid
    start program = "/etc/init.d/myapp start"
    stop program = "/etc/init.d/myapp stop"
    if failed host 127.0.0.1 port 8080 then restart
```

**Problems:**
- Written in C, but with complex state management
- Configuration-driven (what if config is wrong?)
- HTTP health checks conflate "responding" with "healthy"
- PID file race conditions
- No formal guarantees

### Custom Shell Scripts

```bash
#!/bin/bash
while true; do
    if ! pgrep -x "myapp" > /dev/null; then
        echo "Process dead! Restarting..."
        /etc/init.d/myapp start
    fi
    sleep 10
done
```

**Problems:**
- What if `pgrep` fails?
- What if the process exists but is hung?
- What if the script itself dies?
- What if time jumps (NTP sync, VM migration)?
- Race conditions everywhere
- No guarantees whatsoever

### Kubernetes Liveness Probes

```yaml
livenessProbe:
  httpGet:
    path: /healthz
    port: 8080
  initialDelaySeconds: 3
  periodSeconds: 3
```

**Problems:**
- Requires HTTP endpoint (not all processes have one)
- Network failures conflated with process failures
- "initialDelaySeconds" is a guess
- The kubelet doing the checking has its own failure modes
- Abstraction layers all the way down

---

## The Core Problem

All of these tools share the same fundamental flaw:

> **They check symptoms, not state.**

"Can I connect to port 8080?" is not the same as "Is this process alive and functioning correctly?"

And none of them can answer the deeper question:

> **How do I *know* this answer is correct?**

---

## What We Actually Need

A liveness monitor that provides **guarantees**, not guesses:

### CONTRACT 1: Soundness
> If we say ALIVE, the process was alive within the timeout window.
> **We never lie about being alive.**

### CONTRACT 2: Liveness  
> If a process dies, we eventually report DEAD.
> **We never miss a death forever.**

### CONTRACT 3: Stability
> We don't flap. Small timing variations don't cause false state changes.
> **We don't cry wolf.**

These aren't aspirations. They're **contracts** — mathematically provable properties that our implementation **must** satisfy.

---

## Our Constraints

We're building for the real world, which means handling:

| Constraint | Reality |
|------------|---------|
| **Finite polling** | We can't check continuously — only at intervals |
| **Clock issues** | Clocks jump, wrap, drift, and lie |
| **Initialisation** | Systems take time to start up |
| **Atomicity** | Operations can be interrupted |
| **Memory safety** | Corruption happens |
| **Fail-safe** | When in doubt, assume the worst |

A toy implementation ignores these. A **safety-critical** implementation handles every single one.

---

## What We're NOT Building

Let's be clear about scope:

| In Scope | Out of Scope |
|----------|--------------|
| Heartbeat-based liveness detection | HTTP health checks |
| Binary state: ALIVE or DEAD | Graduated health scores |
| Single-process monitoring | Distributed consensus |
| Provably correct state machine | Auto-restart capabilities |
| ~200 lines of C | Feature-rich process manager |

We're building **one thing**, and building it **right**.

---

## Success Criteria

By the end of this course, you will have built a liveness monitor that:

1. **Compiles to <10KB** — No bloat
2. **Has zero dependencies** — Just libc
3. **Handles clock wrap** — Works past year 2038, 2106, whenever
4. **Handles clock jumps** — NTP sync won't break it
5. **Is mathematically proven** — Contracts verified before code written
6. **Is fully tested** — Every state transition exercised
7. **Fails safe** — Unknown conditions → report DEAD, not ALIVE
8. **You understand completely** — Every line traceable to a requirement

---

## The Path Forward

Here's how we'll get there:

```
┌─────────────────────────────────────────────────────────┐
│ Lesson 1: The Problem (you are here)                    │
│   → Understand what we're solving and why               │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│ Lesson 2: Mathematical Closure                          │
│   → Define states, transitions, prove contracts         │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│ Lesson 3: Structs & Data Dictionary                     │
│   → Design the data structures that embody the proof    │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│ Lesson 4: Code                                          │
│   → Transcribe the math into C (it writes itself)       │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│ Lesson 5: Testing & Hardening                           │
│   → Prove the code matches the math                     │
└─────────────────────────────────────────────────────────┘
```

---

## The Key Insight

Most tutorials teach you to write code that **seems to work**.

This course teaches you to write code that **provably works**.

The difference?

```c
// Seems to work
if (time() - last_seen > TIMEOUT) {
    state = DEAD;
}

// Provably works
// (see Lesson 2 for why this is harder than it looks)
```

The first version has at least **three bugs**. Can you spot them?

Hints:
- What type does `time()` return?
- What happens when `time()` wraps?
- What if `last_seen` is in the future?

If you're not sure, **good**. That's why we need Lesson 2.

---

## Exercises

Before moving to Lesson 2, think about these:

### Exercise 1.1: Failure Modes
List 5 ways a simple `while(1) { check(); sleep(10); }` monitor could fail to detect a dead process.

### Exercise 1.2: The Hospital Monitor
A heart monitor in a hospital ICU beeps when it detects no heartbeat. Which is worse:
- (a) Beeping when the patient is fine (false positive)
- (b) Not beeping when the patient is dying (false negative)

How does this inform our CONTRACT 1 (Soundness)?

### Exercise 1.3: Clock Thought Experiment
Your server's clock suddenly jumps forward by 1 hour (NTP correction). What happens to:
- `time() - last_heartbeat > TIMEOUT`
- A heartbeat that was "10 seconds ago" is now "1 hour and 10 seconds ago"

Is the process actually dead?

### Exercise 1.4: Audit
Look at the systemd source code for service watchdog handling. How many lines of code are involved? How many states? Could you personally verify its correctness?

(This isn't a trick question — the answer should motivate why we're building something smaller.)

---

## Summary

**The Problem:**
> Existing liveness monitors are either too complex to verify, too simple to be correct, or both.

**Our Solution:**
> A minimal, mathematically-proven state machine that provides guarantees, not guesses.

**The Method:**
> Prove first, code second. The implementation is just transcription.

---

## Next Lesson

In **Lesson 2: Mathematical Closure**, we'll:
- Define our state space formally
- Map every possible input to a defined output
- Prove our three contracts hold
- Handle clock wrap, initialisation, and faults

No code yet. Just math. The code comes later — and when it does, it will be almost trivial.

---

> *"Don't learn to code. Learn to prove, then transcribe."*

---

[Next: Lesson 2 — Mathematical Closure →](../02-mathematical-closure/LESSON.md)
