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
 *   4. FAULT-STICKY: Once faulted, stay DEAD until reset
 * 
 * REQUIREMENTS:
 *   - Single-writer access (caller must ensure)
 *   - Monotonic time source (caller provides)
 *   - Polling at bounded intervals (caller ensures)
 * 
 * THREAD SAFETY:
 *   This module is NOT thread-safe. The reentrancy guard detects
 *   recursive calls from the SAME thread (e.g., signal handlers),
 *   not concurrent access from multiple threads.
 * 
 *   For multi-threaded use, the caller must provide external
 *   synchronisation (mutex, spinlock, etc.) around all calls
 *   to hb_step() on the same hb_fsm_t instance.
 * 
 * See: lessons/02-mathematical-closure/LESSON.md for proofs
 *      lessons/03-structs/LESSON.md for data dictionary
 * 
 * Copyright (c) 2025 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#ifndef PULSE_H
#define PULSE_H

#include <stdint.h>

/**
 * Visible states of the liveness monitor.
 * 
 * Zero-initialisation yields STATE_UNKNOWN (safe default).
 */
typedef enum {
    STATE_UNKNOWN = 0,  /* No evidence yet                    */
    STATE_ALIVE   = 1,  /* Recent heartbeat observed          */
    STATE_DEAD    = 2   /* Timeout expired or fault detected  */
} state_t;

/**
 * Heartbeat Finite State Machine structure.
 * 
 * INVARIANTS:
 *   INV-1: st ∈ { UNKNOWN, ALIVE, DEAD }
 *   INV-2: (st == ALIVE) → (have_hb == 1)
 *   INV-3: (fault_time ∨ fault_reentry) → (st == DEAD)
 *   INV-4: (in_step == 0) when not executing hb_step
 * 
 * NOTE: Faults are "sticky" — once fault_time or fault_reentry
 *       is set, the FSM remains in STATE_DEAD until hb_init()
 *       is called to reset it.
 */
typedef struct {
    state_t  st;            /* Current state ∈ S                    */
    uint64_t t_init;        /* Boot/reset reference time            */
    uint64_t last_hb;       /* Timestamp of most recent heartbeat   */
    uint8_t  have_hb;       /* Evidence flag: ≥1 heartbeat observed */
    uint8_t  fault_time;    /* Fault: clock corruption detected     */
    uint8_t  fault_reentry; /* Fault: atomicity violation detected  */
    uint8_t  in_step;       /* Reentrancy guard                     */
} hb_fsm_t;

/**
 * Initialise the state machine.
 * 
 * This resets ALL state including fault flags. Use this to
 * recover from a faulted state after investigating the cause.
 * 
 * @param m   Pointer to state machine structure
 * @param now Current timestamp from monotonic source
 */
void hb_init(hb_fsm_t *m, uint64_t now);

/**
 * Execute one atomic step of the state machine.
 * 
 * NOTE: If the FSM is in a faulted state (fault_time or
 *       fault_reentry set), this function will keep the
 *       state as STATE_DEAD. Call hb_init() to reset.
 * 
 * @param m       Pointer to initialised state machine
 * @param now     Current timestamp
 * @param hb_seen 1 if heartbeat observed this step, 0 otherwise
 * @param T       Timeout threshold (time units)
 * @param W       Initialisation window (time units)
 */
void hb_step(hb_fsm_t *m, uint64_t now, uint8_t hb_seen,
             uint64_t T, uint64_t W);

/** Query current state. */
static inline state_t hb_state(const hb_fsm_t *m) {
    return m->st;
}

/** Check if any fault has been detected. */
static inline uint8_t hb_faulted(const hb_fsm_t *m) {
    return m->fault_time || m->fault_reentry;
}

/** Check if evidence has ever been observed. */
static inline uint8_t hb_has_evidence(const hb_fsm_t *m) {
    return m->have_hb;
}

#endif /* PULSE_H */
