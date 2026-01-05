# Lesson 5: Testing & Hardening

## Prove the Code Matches the Math

> "Testing isn't about finding bugs — it's about verifying contracts."

We've proven our design mathematically (Lesson 2), designed our data structures carefully (Lesson 3), and transcribed to code (Lesson 4). Now we verify that the code actually implements what we proved.

---

## The Testing Philosophy

Traditional testing asks: *"Does this code work?"*

Contract testing asks: *"Does this code satisfy its mathematical guarantees?"*

The difference is profound:

| Traditional Testing | Contract Testing |
|---------------------|------------------|
| Test happy paths | Test invariants |
| Test some edge cases | Test ALL boundaries |
| Hope for coverage | Demand proof |
| "It passed" | "It satisfies CONTRACT-1" |

---

## Test Categories

We'll build five types of tests:

1. **Contract tests** — Verify each formal contract
2. **Boundary tests** — Test at T-1, T, T+1
3. **Invariant tests** — Verify struct invariants always hold
4. **Fault injection** — Force failures, verify fail-safe
5. **Fuzz tests** — Random inputs, verify no contract violations

---

## Category 1: Contract Tests

### CONTRACT-1: Soundness

> "Never report ALIVE if actually dead"

**Test strategy**: After timeout, verify ALIVE is never reported.

```c
void test_contract1_soundness(void)
{
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    hb_init(&m, 0);
    
    /* Send one heartbeat at t=0 */
    hb_step(&m, 0, 1, T, W);
    assert(hb_state(&m) == STATE_ALIVE);
    
    /* Advance past timeout without heartbeat */
    hb_step(&m, T + 1, 0, T, W);
    
    /* CONTRACT-1: Must NOT be ALIVE */
    assert(hb_state(&m) != STATE_ALIVE);
    assert(hb_state(&m) == STATE_DEAD);
    
    printf("CONTRACT-1 (Soundness): PASS\n");
}
```

**Why `T + 1`?**

At exactly `T`, we're still within bounds (`a_hb <= T`). At `T + 1`, we've crossed the threshold. This tests the boundary precisely.

### CONTRACT-2: Liveness

> "Eventually report DEAD if heartbeats stop"

**Test strategy**: Stop heartbeats, verify DEAD is reached within bounded time.

```c
void test_contract2_liveness(void)
{
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    hb_init(&m, 0);
    
    /* Send one heartbeat */
    hb_step(&m, 0, 1, T, W);
    assert(hb_state(&m) == STATE_ALIVE);
    
    /* Simulate time passing, no heartbeats */
    /* CONTRACT-2: Must reach DEAD by T + P (P = polling interval) */
    for (uint64_t t = 100; t <= T + 100; t += 100) {
        hb_step(&m, t, 0, T, W);
    }
    
    assert(hb_state(&m) == STATE_DEAD);
    
    printf("CONTRACT-2 (Liveness): PASS\n");
}
```

### CONTRACT-3: Stability

> "No spurious transitions"

**Test strategy**: With steady heartbeats, state should stay ALIVE.

```c
void test_contract3_stability(void)
{
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    hb_init(&m, 0);
    
    /* Send heartbeats regularly, verify no spurious DEAD */
    for (uint64_t t = 0; t <= 10000; t += 100) {
        hb_step(&m, t, 1, T, W);  /* Heartbeat every 100ms */
        
        /* CONTRACT-3: Should always be ALIVE with regular heartbeats */
        assert(hb_state(&m) == STATE_ALIVE);
    }
    
    printf("CONTRACT-3 (Stability): PASS\n");
}
```

---

## Category 2: Boundary Tests

Boundary testing is critical. Off-by-one errors are the #1 source of bugs in timeout logic.

```c
void test_boundary_conditions(void)
{
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    /* Test at T-1: should be ALIVE */
    hb_init(&m, 0);
    hb_step(&m, 0, 1, T, W);
    hb_step(&m, T - 1, 0, T, W);
    assert(hb_state(&m) == STATE_ALIVE);  /* Age = T-1 <= T */
    
    /* Test at exactly T: should be ALIVE (a_hb <= T) */
    hb_init(&m, 0);
    hb_step(&m, 0, 1, T, W);
    hb_step(&m, T, 0, T, W);
    assert(hb_state(&m) == STATE_ALIVE);  /* Age = T <= T */
    
    /* Test at T+1: should be DEAD */
    hb_init(&m, 0);
    hb_step(&m, 0, 1, T, W);
    hb_step(&m, T + 1, 0, T, W);
    assert(hb_state(&m) == STATE_DEAD);   /* Age = T+1 > T */
    
    printf("Boundary tests: PASS\n");
}
```

**The three boundaries:**

| Time | Age | Condition | Expected |
|------|-----|-----------|----------|
| T-1 | T-1 | T-1 ≤ T | ALIVE |
| T | T | T ≤ T | ALIVE |
| T+1 | T+1 | T+1 > T | DEAD |

---

## Category 3: Invariant Tests

From Lesson 3, our invariants:

- **INV-1**: `st ∈ { UNKNOWN, ALIVE, DEAD }`
- **INV-2**: `(st == ALIVE) → (have_hb == 1)`
- **INV-3**: `(fault_time ∨ fault_reentry) → (st == DEAD)`
- **INV-4**: `(in_step == 0)` after `hb_step` returns

```c
void verify_invariants(const hb_fsm_t *m)
{
    /* INV-1: Valid state */
    assert(m->st == STATE_UNKNOWN || 
           m->st == STATE_ALIVE || 
           m->st == STATE_DEAD);
    
    /* INV-2: ALIVE requires evidence */
    if (m->st == STATE_ALIVE) {
        assert(m->have_hb == 1);
    }
    
    /* INV-3: Fault implies DEAD */
    if (m->fault_time || m->fault_reentry) {
        assert(m->st == STATE_DEAD);
    }
    
    /* INV-4: Not in step */
    assert(m->in_step == 0);
}

void test_invariants_hold(void)
{
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    hb_init(&m, 0);
    verify_invariants(&m);
    
    /* Test through various state transitions */
    hb_step(&m, 0, 1, T, W);
    verify_invariants(&m);
    
    hb_step(&m, 500, 0, T, W);
    verify_invariants(&m);
    
    hb_step(&m, T + 1, 0, T, W);
    verify_invariants(&m);
    
    hb_step(&m, T + 2, 1, T, W);  /* Recovery */
    verify_invariants(&m);
    
    printf("Invariant tests: PASS\n");
}
```

---

## Category 4: Fault Injection

### Clock Corruption

Simulate a clock jump that makes age invalid.

```c
void test_clock_corruption(void)
{
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    hb_init(&m, 1000);
    hb_step(&m, 1000, 1, T, W);
    assert(hb_state(&m) == STATE_ALIVE);
    
    /* Simulate clock jump backwards: now < last_hb by huge amount */
    /* This makes age > 2^63 (invalid) */
    hb_step(&m, 500, 0, T, W);  /* now=500, last_hb=1000 */
    
    /* Age = 500 - 1000 = huge number due to unsigned wrap */
    /* Should trigger fault_time and DEAD */
    assert(hb_state(&m) == STATE_DEAD);
    assert(hb_faulted(&m) == 1);
    
    printf("Clock corruption test: PASS\n");
}
```

### Reentrancy Detection

```c
void test_reentrancy_detection(void)
{
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    hb_init(&m, 0);
    hb_step(&m, 0, 1, T, W);
    
    /* Simulate reentrancy by manually setting in_step */
    m.in_step = 1;
    
    hb_step(&m, 100, 1, T, W);
    
    /* Should detect reentry and fail safe */
    assert(hb_state(&m) == STATE_DEAD);
    assert(m.fault_reentry == 1);
    
    printf("Reentrancy detection test: PASS\n");
}
```

---

## Category 5: Fuzz Testing

Generate random inputs and verify contracts are never violated.

```c
#include <stdlib.h>
#include <time.h>

void test_fuzz(uint64_t iterations)
{
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    uint64_t now = 0;
    
    srand((unsigned int)time(NULL));
    
    hb_init(&m, 0);
    
    for (uint64_t i = 0; i < iterations; i++) {
        /* Random heartbeat (0 or 1) */
        uint8_t hb = (uint8_t)(rand() % 2);
        
        /* Random time advance (1-500) */
        now += (uint64_t)(1 + rand() % 500);
        
        state_t old_state = hb_state(&m);
        uint64_t old_last_hb = m.last_hb;
        
        hb_step(&m, now, hb, T, W);
        
        /* Verify invariants after every step */
        verify_invariants(&m);
        
        /* Verify CONTRACT-1: If ALIVE, evidence must be fresh */
        if (hb_state(&m) == STATE_ALIVE) {
            uint64_t age = now - m.last_hb;
            assert(age <= T);
            assert(m.have_hb == 1);
        }
        
        /* Verify CONTRACT-2: If no heartbeat for > T, must be DEAD */
        if (m.have_hb && !hb) {
            uint64_t age = now - m.last_hb;
            if (age > T) {
                assert(hb_state(&m) == STATE_DEAD);
            }
        }
    }
    
    printf("Fuzz test (%lu iterations): PASS\n", iterations);
}
```

---

## The Complete Test Suite

```c
/**
 * test_contracts.c - Contract verification for pulse
 */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include "../src/pulse.h"

/* Include all test functions from above... */

int main(void)
{
    printf("=== Pulse Contract Test Suite ===\n\n");
    
    /* Contract tests */
    test_contract1_soundness();
    test_contract2_liveness();
    test_contract3_stability();
    
    /* Boundary tests */
    test_boundary_conditions();
    
    /* Invariant tests */
    test_invariants_hold();
    
    /* Fault injection */
    test_clock_corruption();
    test_reentrancy_detection();
    
    /* Fuzz testing */
    test_fuzz(100000);
    
    printf("\n=== All tests PASSED ===\n");
    return 0;
}
```

---

## Hardening Checklist

Beyond testing, we apply defensive compilation:

### Compiler Flags

```makefile
# Warnings as errors
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c99

# Extra warnings
CFLAGS += -Wshadow -Wconversion -Wdouble-promotion
CFLAGS += -Wformat=2 -Wundef -fno-common

# Sanitizers (debug builds)
DEBUG_FLAGS = -fsanitize=undefined,address
```

### Static Analysis

```bash
# cppcheck
cppcheck --enable=all --std=c99 src/pulse.c

# clang-tidy
clang-tidy src/pulse.c -- -std=c99

# scan-build (Clang static analyzer)
scan-build make
```

### Memory Checking

```bash
# Valgrind
valgrind --leak-check=full --error-exitcode=1 ./build/pulse

# AddressSanitizer (compile with -fsanitize=address)
./build/pulse_asan
```

### Final Checklist

- [ ] Compiles with `-Wall -Wextra -Werror -pedantic`
- [ ] No compiler warnings on any platform
- [ ] No dynamic allocation (`grep -r malloc`)
- [ ] Valgrind clean (no leaks, no errors)
- [ ] cppcheck clean
- [ ] All contract tests pass
- [ ] All boundary tests pass
- [ ] All fault injection tests pass
- [ ] Fuzz test passes (1M+ iterations)
- [ ] Code coverage > 95%

---

## Running the Tests

```bash
cd projects/pulse

# Build and run tests
make test

# With verbose output
make test VERBOSE=1

# With sanitizers
make clean
CFLAGS="-fsanitize=undefined,address" make test

# With coverage
CFLAGS="--coverage" make test
gcov src/pulse.c
```

---

## Exercises

### Exercise 5.1: Coverage Gap

Write a test that exercises the path where `hb_step` is called with `hb_seen = 1` but then immediately called again with `hb_seen = 0` before timeout.

### Exercise 5.2: Property-Based Testing

Using a property-based testing framework (like [theft](https://github.com/silentbicycle/theft) for C), write a property that states: "For any sequence of inputs, if the final heartbeat was more than T ago, state is DEAD."

### Exercise 5.3: Mutation Testing

Introduce a bug in `pulse.c` (e.g., change `>` to `>=` in the timeout check). Does your test suite catch it? If not, add a test that would.

### Exercise 5.4: Cross-Platform

Compile and test on:
- Linux x86_64
- Linux ARM64
- macOS
- FreeBSD

Are there any platform-specific issues?

### Exercise 5.5: Benchmarking

Write a benchmark that measures:
- Time per `hb_step` call
- Memory footprint of `hb_fsm_t`
- Binary size

Compare with the requirements from Lesson 1.

---

## Key Takeaways

1. **Test contracts, not code paths** — Our tests verify mathematical properties
2. **Boundaries are critical** — T-1, T, T+1 catch off-by-one errors
3. **Invariants must hold** — Check after every operation
4. **Faults must fail safe** — Corruption → DEAD, not ALIVE
5. **Fuzz finds surprises** — Random inputs reveal edge cases
6. **Hardening is defense in depth** — Compiler flags, sanitizers, static analysis

---

## Conclusion

You've now built a complete, mathematically verified, thoroughly tested liveness monitor in C. Let's review what you've accomplished:

| Lesson | What You Built |
|--------|----------------|
| 1 | Problem understanding and failure analysis |
| 2 | Mathematical proof of correctness |
| 3 | Data structures with explicit contracts |
| 4 | Code that transcribes the proof |
| 5 | Tests that verify the transcription |

**The result:**
- ~200 lines of C
- Zero dependencies
- Proven correct
- Thoroughly tested
- Handles all edge cases

**The method:**
> *"Don't learn to code. Learn to prove, then transcribe."*

---

## Where to Go From Here

### Extend Pulse

- Add configurable failure actions (callbacks)
- Support multiple monitored processes
- Add metrics/statistics collection
- Create a daemon wrapper

### Apply the Method

Use the same approach for other small, critical components:
- Rate limiters
- Circuit breakers
- Token buckets
- Ring buffers

### Go Deeper

- Formal verification with Frama-C
- Model checking with CBMC
- Property-based testing with Hypothesis
- Temporal logic proofs

---

## Final Words

You've learned something most programmers never learn: how to **know** your code is correct, not just **hope** it is.

This skill is rare. This skill is valuable. This skill will serve you for your entire career.

Now go build things that don't break.

---

> *"The amateur practices until they get it right. The professional practices until they can't get it wrong."*

---

[Previous: Lesson 4 — Code](../04-code/LESSON.md) | [Back to Project README](../../README.md)
