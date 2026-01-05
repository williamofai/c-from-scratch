/**
 * pulse.h - Heartbeat-Based Liveness Monitor
 * 
 * A closed, total, deterministic state machine for monitoring
 * process liveness via heartbeat signals.
 * 
 * CONTRACTS:
 *   1. SOUNDNESS:  Never report ALIVE if actually dead
 *   2. LIVENESS:   Eventually report DEAD if heartbeats stop
 *   3. STABILITY:  No spurious transitions
 * 
 * REQUIREMENTS:
 *   - Single-writer access (caller must ensure)
 *   - Monotonic time source (caller provides)
 *   - Polling at bounded intervals (caller ensures)
 * 
 * MATHEMATICAL BASIS:
 *   State Space:   S = { UNKNOWN, ALIVE, DEAD }
 *   Time Domain:   t ∈ ℤ₂⁶⁴ (modular, wrapping)
 *   Transition:    δ: S × I → S (total function)
 * 
 * See: lessons/02-mathematical-closure/LESSON.md
 *      lessons/03-structs/LESSON.md
 */

#ifndef PULSE_H
#define PULSE_H

#include <stdint.h>

/*============================================================================
 * STATE ENUMERATION
 *============================================================================
 * 
 * Why enum (not #define)?
 *   - Self-documenting: compiler knows valid values
 *   - Debugger-friendly: shows STATE_ALIVE instead of 1
 *   - Switch completeness: compiler can warn on missing cases
 * 
 * Why these values?
 *   - 0 = UNKNOWN: Zero-init gives safe default
 *   - 1 = ALIVE:   Non-zero = active (C idiom)
 *   - 2 = DEAD:    Distinct from both
 */
typedef enum {
    STATE_UNKNOWN = 0,  /* No evidence yet (initial/safe state)     */
    STATE_ALIVE   = 1,  /* Recent heartbeat observed                */
    STATE_DEAD    = 2   /* Timeout expired or fault detected        */
} state_t;

/*============================================================================
 * STATE MACHINE STRUCTURE
 *============================================================================
 * 
 * All fields explicitly owned—no external references.
 * No dynamic allocation. Fixed size. Fully deterministic.
 * 
 * INVARIANTS (must hold outside hb_step execution):
 *   INV-1: st ∈ { UNKNOWN, ALIVE, DEAD }
 *   INV-2: (st == ALIVE) → (have_hb == 1)
 *   INV-3: (fault_time ∨ fault_reentry) → (st == DEAD)
 *   INV-4: in_step == 0
 * 
 * MEMORY LAYOUT (typical 64-bit system):
 *   Offset 0:   st            (4 bytes, enum)
 *   Offset 4:   [padding]     (4 bytes, alignment)
 *   Offset 8:   t_init        (8 bytes, uint64_t)
 *   Offset 16:  last_hb       (8 bytes, uint64_t)
 *   Offset 24:  have_hb       (1 byte)
 *   Offset 25:  fault_time    (1 byte)
 *   Offset 26:  fault_reentry (1 byte)
 *   Offset 27:  in_step       (1 byte)
 *   Offset 28:  [padding]     (4 bytes, struct alignment)
 *   Total:      32 bytes
 */
typedef struct {
    /*------------------------------------------------------------------------
     * st: Current State
     *------------------------------------------------------------------------
     * PURPOSE:    Represents mathematical state S
     * CONSTRAINT: Must be one of { UNKNOWN, ALIVE, DEAD }
     * CONTRACT:   Directly implements state space from Lesson 2
     */
    state_t  st;

    /*------------------------------------------------------------------------
     * t_init: Initialisation Time
     *------------------------------------------------------------------------
     * PURPOSE:    Reference point for "time since boot"
     * CONSTRAINT: Set once at init, never modified after
     * CONTRACT:   Required for init window (W) computation
     * 
     * Why uint64_t?
     *   - Exact 64-bit width (N=64 in our math)
     *   - Unsigned: defined overflow (wraps at 2^64)
     *   - Portable: same size on all platforms
     */
    uint64_t t_init;

    /*------------------------------------------------------------------------
     * last_hb: Last Heartbeat Time
     *------------------------------------------------------------------------
     * PURPOSE:    Reference point for "time since heartbeat"
     * CONSTRAINT: Updated each time hb_seen=1
     * CONTRACT:   Required for timeout (T) computation (CONTRACT-2)
     * 
     * Age computed as: a_hb = (now - last_hb) mod 2^64
     * Valid if: a_hb < 2^63 (half-range rule)
     */
    uint64_t last_hb;

    /*------------------------------------------------------------------------
     * have_hb: Evidence Flag
     *------------------------------------------------------------------------
     * PURPOSE:    Tracks whether ANY heartbeat has been observed
     * CONSTRAINT: 0 = no evidence, 1 = evidence exists
     * CONTRACT:   Distinguishes UNKNOWN (no evidence) from DEAD (expired)
     * 
     * Why separate flag (not sentinel in last_hb)?
     *   - What if first heartbeat arrives at time 0?
     *   - What if clock wraps to 0?
     *   - Don't overload semantics
     * 
     * Why uint8_t (not bool)?
     *   - bool requires <stdbool.h>, size varies
     *   - uint8_t: exactly 1 byte, universal support
     */
    uint8_t  have_hb;

    /*------------------------------------------------------------------------
     * fault_time: Clock Corruption Flag
     *------------------------------------------------------------------------
     * PURPOSE:    Records that time arithmetic failed validity check
     * CONSTRAINT: Once set, never cleared (sticky fault)
     * CONTRACT:   Forces DEAD state (fail-safe for CONTRACT-1)
     * 
     * Triggered when: age >= 2^63 (half-range violation)
     * Causes: clock jump backwards, extreme forward jump, corruption
     */
    uint8_t  fault_time;

    /*------------------------------------------------------------------------
     * fault_reentry: Atomicity Violation Flag
     *------------------------------------------------------------------------
     * PURPOSE:    Records that hb_step was called while already executing
     * CONSTRAINT: Once set, never cleared (sticky fault)
     * CONTRACT:   Forces DEAD state (struct may be inconsistent)
     * 
     * Why not use mutexes?
     *   - Not available on all platforms (bare metal, some RTOS)
     *   - Our approach: detect and fail-safe, don't prevent
     */
    uint8_t  fault_reentry;

    /*------------------------------------------------------------------------
     * in_step: Reentrancy Guard
     *------------------------------------------------------------------------
     * PURPOSE:    Detects concurrent/recursive calls to hb_step
     * CONSTRAINT: 1 only during hb_step execution, 0 otherwise
     * CONTRACT:   Enables fault_reentry detection
     * 
     * Pattern:
     *   if (in_step) { fault! }
     *   in_step = 1;
     *   ... work ...
     *   in_step = 0;
     */
    uint8_t  in_step;

} hb_fsm_t;

/*============================================================================
 * FUNCTION: hb_init
 *============================================================================
 * Initialise the state machine to a known, safe state.
 * 
 * PRECONDITIONS:
 *   - m points to valid, writable memory for hb_fsm_t
 *   - now is current time from a monotonic source
 * 
 * POSTCONDITIONS:
 *   - m->st == STATE_UNKNOWN (safe default)
 *   - m->t_init == now
 *   - m->last_hb == 0 (undefined until first heartbeat)
 *   - m->have_hb == 0 (no evidence)
 *   - m->fault_time == 0 (no faults)
 *   - m->fault_reentry == 0 (no faults)
 *   - m->in_step == 0 (not executing)
 * 
 * All invariants satisfied after call.
 * 
 * @param m   Pointer to state machine structure
 * @param now Current timestamp from monotonic source
 */
void hb_init(hb_fsm_t *m, uint64_t now);

/*============================================================================
 * FUNCTION: hb_step
 *============================================================================
 * Execute one atomic step of the state machine.
 * 
 * This is the core transition function δ from Lesson 2.
 * 
 * PRECONDITIONS:
 *   - m was initialised via hb_init()
 *   - now is current time (monotonic, may wrap)
 *   - hb_seen ∈ {0, 1}
 *   - T > 0 (timeout threshold)
 *   - W >= 0 (init window, may be 0)
 *   - No concurrent calls (single-writer)
 * 
 * POSTCONDITIONS:
 *   - m->st updated per transition table
 *   - If hb_seen: m->last_hb = now, m->have_hb = 1
 *   - If fault detected: m->st = STATE_DEAD, fault flag set
 * 
 * CONTRACTS PRESERVED:
 *   CONTRACT-1: ALIVE only if have_hb && a_hb <= T && time valid
 *   CONTRACT-2: DEAD if a_hb > T (when have_hb)
 *   CONTRACT-3: Transition only per table (deterministic)
 * 
 * TRANSITION TABLE:
 *   Current   hb_seen  Condition      Next     Justification
 *   -------   -------  ---------      ----     -------------
 *   ANY       ANY      age invalid    DEAD     Fail-safe
 *   UNKNOWN   0        a_init < W     UNKNOWN  Init latency
 *   UNKNOWN   0        a_init >= W    UNKNOWN  No evidence
 *   UNKNOWN   1        a_hb <= T      ALIVE    First evidence
 *   ALIVE     1        a_hb <= T      ALIVE    Invariant
 *   ALIVE     0        a_hb > T       DEAD     Timeout
 *   DEAD      1        a_hb <= T      ALIVE    Recovery
 *   DEAD      0        a_hb > T       DEAD     Stability
 * 
 * @param m       Pointer to initialised state machine
 * @param now     Current timestamp
 * @param hb_seen 1 if heartbeat observed this step, 0 otherwise
 * @param T       Timeout threshold (time units)
 * @param W       Initialisation window (time units)
 */
void hb_step(hb_fsm_t *m, uint64_t now, uint8_t hb_seen,
             uint64_t T, uint64_t W);

/*============================================================================
 * INLINE HELPERS
 *============================================================================
 * These are accessors that don't modify state.
 * Inlined for zero overhead.
 */

/**
 * Query current state.
 * 
 * @param m Pointer to state machine
 * @return  Current state (UNKNOWN, ALIVE, or DEAD)
 */
static inline state_t hb_state(const hb_fsm_t *m)
{
    return m->st;
}

/**
 * Check if any fault has been detected.
 * 
 * A faulted monitor should be considered unreliable.
 * The state will be DEAD, but the cause was abnormal.
 * 
 * @param m Pointer to state machine
 * @return  1 if fault detected (time or reentry), 0 otherwise
 */
static inline uint8_t hb_faulted(const hb_fsm_t *m)
{
    return m->fault_time || m->fault_reentry;
}

/**
 * Check if evidence of life has ever been observed.
 * 
 * Useful for distinguishing:
 *   - UNKNOWN because we're waiting vs
 *   - UNKNOWN because nothing happened
 * 
 * @param m Pointer to state machine
 * @return  1 if at least one heartbeat observed, 0 otherwise
 */
static inline uint8_t hb_has_evidence(const hb_fsm_t *m)
{
    return m->have_hb;
}

#endif /* PULSE_H */
