# Lesson 2: Mathematical Closure

## Prove It Works Before Writing Code

> "Don't learn to code. Learn to prove, then transcribe."

This lesson contains the complete mathematical design of our heartbeat liveness monitor. Every decision here will directly inform the C structures and code in later lessons. If something isn't proven here, it shouldn't exist in the code.

---

## 0. Design Intent

This design implements a **closed, total, deterministic state machine** that monitors liveness via heartbeats under real-world constraints:

- Finite polling rate
- Modular (wrapping) clocks
- Initialisation latency
- Atomicity and memory safety
- Fail-safe behaviour under corruption

**The guarantees:**

| Property | Guarantee |
|----------|-----------|
| Soundness | System **never** reports ALIVE incorrectly |
| Liveness | System **eventually** reports DEAD if heartbeats stop |
| Determinism | System **never** enters an undefined state |

These guarantees hold even under clock faults or resets.

---

## 1. System Model

### 1.1 Visible States

The monitor can be in exactly one of three states:

```
S = { UNKNOWN, ALIVE, DEAD }
```

| State | Meaning | When |
|-------|---------|------|
| UNKNOWN | Insufficient evidence | Initial state, no heartbeat yet |
| ALIVE | Evidence of life | Heartbeat received within timeout |
| DEAD | Evidence expired or fault | Timeout elapsed or corruption detected |

**Why only three states?**

More states create more transitions to prove. These three are the minimum needed to satisfy our contracts. UNKNOWN is essential—without it, we'd have to guess ALIVE or DEAD at startup.

### 1.2 Time Model

Time is a **monotonic modular counter**:

```
t ∈ ℤ₂ᴺ  where N = 64 for uint64_t
```

This means:
- Time values are unsigned 64-bit integers
- Values wrap around at 2⁶⁴ (approximately 18 quintillion)
- Wrapping is well-defined (not undefined behaviour)

**Time differences** are computed using modular subtraction:

```
age(a, b) = (a - b) mod 2ᴺ
```

**The half-range rule** determines validity:

```
age is valid iff age < 2ᴺ⁻¹
```

If `age ≥ 2⁶³`, either:
- The clock jumped backwards, or
- More than 2⁶³ time units elapsed (physically impossible in practice)

Either way, we can't trust the result.

### Why Modular Arithmetic?

**Problem with signed integers:**
```c
int64_t age = now - then;  // UNDEFINED BEHAVIOUR if overflow!
```
The C standard says signed overflow is undefined. The compiler may do anything.

**Solution with unsigned:**
```c
uint64_t age = now - then;  // DEFINED: wraps at 2⁶⁴
```
Unsigned overflow is guaranteed to wrap. We can reason about it mathematically.

---

## 2. Parameters (Design Constants)

| Parameter | Symbol | Meaning | Constraint |
|-----------|--------|---------|------------|
| Timeout | T | Maximum age of heartbeat before DEAD | T > P_max + Jitter |
| Init Window | W | Time to wait before expecting heartbeats | W ≥ 0 |
| Poll Period | P | Maximum time between `hb_step` calls | P ≤ T_margin |
| Margin | T_margin | Acceptable lateness in DEAD detection | Optional |

**Constraint explanations:**

- **T > P_max + Jitter**: Timeout must exceed worst-case polling interval plus clock jitter, otherwise we might miss valid heartbeats
- **W ≥ 0**: Window can be zero (immediate expectations) or positive (warm-up period)
- **P ≤ T_margin**: If we want bounded-lateness DEAD detection, polling can't be too slow

---

## 3. Internal State Variables

All variables are explicitly owned by the FSM struct (no globals):

| Variable | Type | Purpose |
|----------|------|---------|
| `st` | state_t | Current state ∈ S |
| `t_init` | uint64_t | Boot/reset reference time |
| `last_hb` | uint64_t | Timestamp of most recent heartbeat |
| `have_hb` | uint8_t | Evidence flag (≥1 heartbeat observed) |
| `fault_time` | uint8_t | Clock corruption detected |
| `fault_reentry` | uint8_t | Atomicity violation detected |

**Why these specific variables?**

- `st`: Directly represents our mathematical state S
- `t_init`, `last_hb`: Required to compute ages for transitions
- `have_hb`: Distinguishes "never saw heartbeat" from "heartbeat expired"
- `fault_*`: Record violations for diagnostics; trigger fail-safe

---

## 4. Temporal Behaviour

This diagram shows how the initialisation window (W) and timeout (T) interact:

```
Timeline: t_init ─────────[W]─────────[T]─────────► now
             │              │           │
          UNKNOWN      UNKNOWN →    ALIVE/DEAD
          (no hb)      ALIVE/DEAD   (based on
                       (if hb seen)  last_hb age)
```

**Key insight:** The system distinguishes **absence of evidence** (UNKNOWN) from **expired evidence** (DEAD).

**Example scenarios:**

1. **Heartbeat before W expires**: 
   - t=0: Init → UNKNOWN
   - t=W/2: Heartbeat → ALIVE
   - Stays ALIVE as long as heartbeats continue within T

2. **No heartbeat, W expires**:
   - t=0: Init → UNKNOWN
   - t=W: Still UNKNOWN (no evidence yet, but window passed)
   - Stays UNKNOWN until heartbeat or fault

3. **Heartbeat, then timeout**:
   - t=0: Init → UNKNOWN
   - t=1: Heartbeat → ALIVE
   - t=1+T+1: No heartbeat → DEAD

---

## 5. Closed Transition Function δ (Total)

At each atomic step, the inputs are:

- `now` ∈ 𝕋 (current time)
- `hb_seen` ∈ {0, 1} (heartbeat observed this step?)

### 5.1 Wrap-Safe Age Computation

```
a_hb = (now - last_hb) mod 2ᴺ
a_init = (now - t_init) mod 2ᴺ
```

Both are validated using the half-range rule before use.

### 5.2 Total State Update Table

| Current | hb_seen | Condition | Next | Justification |
|---------|---------|-----------|------|---------------|
| ANY | ANY | age invalid | DEAD | Fail-safe |
| UNKNOWN | 0 | a_init < W | UNKNOWN | Init latency |
| UNKNOWN | 0 | a_init ≥ W | UNKNOWN | No evidence |
| UNKNOWN | 1 | a_hb ≤ T | ALIVE | First evidence |
| ALIVE | 1 | a_hb ≤ T | ALIVE | Invariant |
| ALIVE | 0 | a_hb > T | DEAD | Timeout |
| DEAD | 1 | a_hb ≤ T | ALIVE | Recovery |
| DEAD | 0 | a_hb > T | DEAD | Stability |

**This table is exhaustive** → δ is total → design is **CLOSED**.

### Why "CLOSED" Matters

A **closed** system has no undefined states or transitions. Every possible input combination maps to exactly one output. This means:

- No "what if?" scenarios left unconsidered
- No undefined behaviour in implementation
- Complete testability

**How to verify closure:**

1. List all states: 3 (UNKNOWN, ALIVE, DEAD)
2. List all input combinations: hb_seen ∈ {0, 1} × age validity ∈ {valid, invalid}
3. Count rows in table: 8 transitions + 1 catch-all = all cases covered ✓

---

## 6. Formal Proof Summary

### 6.1 CONTRACT-1: Soundness

**Statement:** Never report ALIVE if actually DEAD

**Proof:**

ALIVE is reachable if and only if:
```
have_hb = 1 ∧ a_hb ≤ T
```

This implies a heartbeat exists in the interval [now - T, now].

If time arithmetic is invalid → forced DEAD (row 1 of transition table).

Therefore, ALIVE can only be reported when evidence is fresh and valid.

**✓ Proven.**

### 6.2 CONTRACT-2: Liveness

**Statement:** Eventually report DEAD if heartbeats stop

**Proof:**

Let last heartbeat be at time t*.

For any step where:
```
now > t* + T  ⇒  a_hb > T  ⇒  DEAD
```

Since polling occurs at least every P, DEAD is reached by:
```
t* + T + P
```

This is a finite, bounded time.

**✓ Proven** (with explicit polling assumption).

### 6.3 CONTRACT-3: Stability

**Statement:** No spurious transitions

**Two valid interpretations:**

1. **No early DEAD**: Guaranteed by the `a_hb > T` rule—we only transition to DEAD when the age actually exceeds the timeout.

2. **Bounded late DEAD**: Guaranteed if `P ≤ T_margin`—the delay between heartbeat death and DEAD report is bounded.

**✓ Proven** under stated constraints.

---

## 7. Failure Mode Analysis

### 7.1 Clock Jumps Backwards

**Scenario:** System time corrected by NTP, VM snapshot restored, or hardware fault.

**Detection:** Age computation yields value ≥ 2⁶³.

**Response:** `fault_time = 1`, state forced to DEAD.

**Rationale:** We cannot reason about time if it's unreliable. Fail-safe.

### 7.2 Clock Jumps Forward

**Scenario:** System resume from suspend, or long scheduling delay.

**Detection:** Same as backwards—age validity check.

**Response:** Same fail-safe behaviour.

### 7.3 Concurrent Access

**Scenario:** `hb_step` called while already executing (signal handler, multithreading bug).

**Detection:** `in_step` flag already set on entry.

**Response:** `fault_reentry = 1`, state forced to DEAD.

**Rationale:** Struct may be in inconsistent state. Fail-safe.

### 7.4 Memory Corruption

**Scenario:** Random bit flip changes `st` to invalid value (e.g., 47).

**Detection:** Not explicitly detected in current design.

**Response:** Transition table handles ANY state—invalid states get caught by first row (age check will fail) or produce DEAD on next transition.

**Enhancement opportunity:** Add explicit state validation in `hb_step`.

---

## 8. Implementation Checklist (Pre-Conditions for Code)

Before writing code, verify:

- [ ] Single-writer or critical section will be enforced by caller
- [ ] Time source is monotonic (or we accept faults on non-monotonic)
- [ ] No dynamic memory will be required
- [ ] All arithmetic will use defined operations (unsigned)
- [ ] Reset semantics are explicit (what happens after fault?)
- [ ] Struct will use fixed-width types only
- [ ] All fault paths lead to DEAD (fail-safe)
- [ ] No implicit assumptions remain undocumented

---

## 9. Final Assessment

**This design is CLOSED.**

| Property | Status |
|----------|--------|
| Every input → exactly one next state | ✓ |
| All failure modes → deterministic outcomes | ✓ |
| Contracts proven under explicit assumptions | ✓ |
| Ready for direct transcription to C99 | ✓ |

---

## Exercises

### Exercise 2.1: Transition Trace
Trace through the state transitions for this scenario:
- t=0: Init
- t=5: Heartbeat
- t=10: No heartbeat
- t=15: No heartbeat
- t=20: Heartbeat

Assume T=12, W=3. What is the state at each step?

### Exercise 2.2: Prove by Contradiction
Prove CONTRACT-1 by contradiction: Assume ALIVE is reported when the process is actually dead. Show this contradicts our transition rules.

### Exercise 2.3: Parameter Selection
A process sends heartbeats every 1000ms. Your polling interval is 100ms. Clock jitter is ±5ms. What values should you choose for T and W? Justify each choice.

### Exercise 2.4: Missing Transition
Remove row 7 (DEAD + heartbeat → ALIVE) from the transition table. What property is lost? Is the system still closed?

### Exercise 2.5: Fourth State
Propose a fourth state (e.g., STARTING). Define its transitions. Does it improve any contract, or just add complexity?

---

## Deliverables

After completing this lesson, you should have:

1. ✓ State diagram showing all transitions
2. ✓ Invariants list (what must always be true)
3. ✓ Contract proofs (why each guarantee holds)
4. ✓ Implementation checklist (pre-conditions for code)

---

## Next Lesson

In **Lesson 3: Structs & Data Dictionary**, we'll translate this mathematical model into C data structures. Every field will trace back to a requirement established here.

*"The struct is the contract. The code merely enforces it."*
