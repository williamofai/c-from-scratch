# c-from-scratch — Module 6: Pressure

## Lesson 6: Composition & Applications

---

## The Complete Safety Stack

All six modules composing into a complete system:

```
┌─────────────────────────────────────────────────────────────┐
│                     APPLICATION                              │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     PRESSURE (Queue)                         │
│   Bounded buffer between producer and consumer               │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     CONSENSUS (Voter)                        │
│   TMR voting on sensor values                                │
└─────────────────────────────────────────────────────────────┘
                              │
          ┌───────────────────┼───────────────────┐
          ▼                   ▼                   ▼
┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
│   CHANNEL 0     │ │   CHANNEL 1     │ │   CHANNEL 2     │
│  Pulse → Base   │ │  Pulse → Base   │ │  Pulse → Base   │
│  → Drift        │ │  → Drift        │ │  → Drift        │
└─────────────────┘ └─────────────────┘ └─────────────────┘
```

---

## Using Pressure for Backpressure

```c
pressure_result_t r;
int err = pressure_enqueue(&q, sensor_value, now, &r);

if (err == PRESSURE_ERR_FULL) {
    sleep_ms(10);  /* Slow down */
}
if (r.state == PRESSURE_CRITICAL) {
    drop_low_priority_work();  /* Shed load */
}
```

---

## Applications

- **Medical Devices**: DROP_OLDEST for real-time ECG
- **Flight Recorders**: DROP_NEWEST to preserve history  
- **Network Buffers**: REJECT for TCP-style backpressure

---

## Framework Complete

| Module | Question | Status |
|--------|----------|--------|
| Pulse | Does it exist? | ✓ |
| Baseline | Is it normal? | ✓ |
| Timing | Is it healthy? | ✓ |
| Drift | Moving toward failure? | ✓ |
| Consensus | Which sensor to trust? | ✓ |
| **Pressure** | **Handle overflow?** | ✓ |

---

## Final Takeaway

> **Good systems don't explode under load. They degrade gracefully.**

**End of Module 6: Pressure — Bounded Queues & Backpressure**
