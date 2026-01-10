# c-from-scratch — Module 7: Mode Manager

## Lesson 1: The Problem

---

## The Indecisive System

You've built six modules that tell you everything about your system's health:

| Module | Question |
|--------|----------|
| Pulse | Is it alive? |
| Baseline | Is it normal? |
| Timing | Is it regular? |
| Drift | Is it trending toward failure? |
| Consensus | Which sensor to trust? |
| Pressure | Is the queue overflowing? |

But **who makes the decisions?**

---

## The Scattered `if` Problem

Without a centralized decision-maker:

```c
/* BAD: Safety logic scattered everywhere */
if (pulse_state == DEAD) {
    stop_motor();  /* Maybe? */
}

/* Somewhere else in the code... */
if (consensus_state == NO_QUORUM) {
    /* What do we do? Who knows? */
}

/* In another file... */
if (drift_slope > threshold) {
    reduce_power();  /* Or should we shut down? */
}
```

This leads to:
- **Inconsistent responses** to the same conditions
- **Race conditions** between subsystems
- **Untestable safety logic** spread across the codebase
- **No audit trail** of what happened and why

---

## Real-World Failures

**Boeing 737 MAX (2018-2019)**:
- Two crashes, 346 deaths
- MCAS system made decisions based on single sensor
- No clear mode transition logic
- Pilots didn't know what "mode" the aircraft was in

**Therac-25 (1985-1987)**:
- Radiation therapy machine
- Race conditions between operator input and machine state
- No centralized mode manager
- Six patients overdosed, three died

**Toyota Unintended Acceleration (2009-2011)**:
- Task scheduling issues
- No clear "safe mode" definition
- System could be in ambiguous states

---

## What We Need

A **Mode Manager** that:

1. **Exists in exactly one mode** at any time
2. **Aggregates all health signals** into a single decision
3. **Controls what actions are allowed** in each mode
4. **Logs all transitions** for audit
5. **Cannot be bypassed** by individual subsystems

```
┌─────────────────────────────────────────────────────────┐
│                    MODE MANAGER                          │
│                   "The Captain"                          │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  Inputs:                                                 │
│    [Pulse] [Baseline] [Timing] [Drift] [Consensus] [Pressure]
│                                                          │
│  Outputs:                                                │
│    Current Mode + Allowed Actions + Audit Log            │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

---

## The Mode Hierarchy

| Mode | Meaning | Actions Allowed |
|------|---------|-----------------|
| **INIT** | Power-on, validating | Log, Communicate |
| **STARTUP** | Learning period | + Calibrate |
| **OPERATIONAL** | Normal operation | + Actuate |
| **DEGRADED** | Approaching limits | Log, Communicate only |
| **EMERGENCY** | Critical fault | Log, Communicate only |
| **TEST** | Maintenance override | Full access |

---

## The Key Insight

> **Modules 1-6 answer "What's happening?"**
> **Module 7 answers "What do we DO about it?"**

Sensors report. The Captain decides.

---

## Next Lesson

We'll define the mathematical model: what inputs trigger what transitions, and how we guarantee safety properties.
