/**
 * timing.h - Composed Timing Health Monitor
 * 
 * A closed, total, deterministic composition of Pulse and Baseline
 * for detecting timing anomalies in event streams.
 * 
 * Module 1 proved existence in time.
 * Module 2 proved normality in value.
 * Module 3 proves health over time.
 * 
 * THE COMPOSITION:
 *   event → Pulse → Δt → Baseline → timing_anomaly?
 * 
 * CONTRACTS:
 *   1. EXISTENCE INHERITANCE:  Dead pulse → Dead timing
 *   2. NORMALITY INHERITANCE:  Timing deviation → Unhealthy
 *   3. HEALTH REQUIRES EVIDENCE: No premature health claims
 *   4. BOUNDED DETECTION:      Anomalies detected in O(1/α) steps
 *   5. SPIKE RESISTANCE:       Single anomaly can't corrupt baseline
 *   6. DETERMINISTIC:          Same inputs → Same outputs
 * 
 * REQUIREMENTS:
 *   - Single-writer access (caller must ensure)
 *   - Monotonic time source (caller provides)
 *   - Events arrive in timestamp order
 * 
 * See: lessons/ for proofs and data dictionary
 * 
 * Copyright (c) 2025 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#ifndef TIMING_H
#define TIMING_H

#include <stdint.h>
#include "pulse.h"
#include "baseline.h"

/**
 * Composed timing health states.
 * 
 * Zero-initialisation yields TIMING_INITIALIZING (safe default).
 */
typedef enum {
    TIMING_INITIALIZING = 0,  /* Learning phase - insufficient evidence  */
    TIMING_HEALTHY      = 1,  /* Normal rhythm - pulse alive, timing stable */
    TIMING_UNHEALTHY    = 2,  /* Timing anomaly - pulse alive, timing deviated */
    TIMING_DEAD         = 3   /* No heartbeat - pulse dead */
} timing_state_t;

/**
 * Configuration parameters for timing monitor.
 * 
 * Combines Pulse and Baseline configuration.
 */
typedef struct {
    /* Pulse configuration */
    uint64_t heartbeat_timeout_ms;  /* T: max time between heartbeats */
    uint64_t init_window_ms;        /* W: max time to first heartbeat */
    
    /* Baseline configuration */
    double   alpha;                 /* EMA smoothing factor ∈ (0,1) */
    double   epsilon;               /* Variance floor for z-score */
    double   k;                     /* Deviation threshold (sigma) */
    uint32_t n_min;                 /* Min observations before STABLE */
} timing_config_t;

/**
 * Default configuration.
 * 
 * heartbeat_timeout_ms = 5000   5 seconds max between heartbeats
 * init_window_ms       = 10000  10 seconds to first heartbeat
 * alpha                = 0.1    Effective window ≈ 20 observations
 * epsilon              = 1e-9   Variance floor
 * k                    = 3.0    Three-sigma threshold
 * n_min                = 20     Learning period
 */
static const timing_config_t TIMING_DEFAULT_CONFIG = {
    .heartbeat_timeout_ms = 5000,
    .init_window_ms       = 10000,
    .alpha                = 0.1,
    .epsilon              = 1e-9,
    .k                    = 3.0,
    .n_min                = 20
};

/**
 * Timing Finite State Machine structure.
 * 
 * INVARIANTS:
 *   INV-1: state ∈ { INITIALIZING, HEALTHY, UNHEALTHY, DEAD }
 *   INV-2: (state == HEALTHY) → (pulse.st == ALIVE ∧ baseline.state == STABLE)
 *   INV-3: (state == DEAD) → (pulse.st == DEAD)
 *   INV-4: (fault_pulse ∨ fault_baseline) → (state ∈ {UNHEALTHY, DEAD})
 *   INV-5: (in_step == 0) when not executing timing_heartbeat/timing_check
 *   INV-6: last_heartbeat_ms is valid after first heartbeat
 */
typedef struct {
    /* Configuration (immutable after init) */
    timing_config_t cfg;
    
    /* Component FSMs (embedded, not referenced) */
    hb_fsm_t   pulse;
    base_fsm_t baseline;
    
    /* Composed state */
    timing_state_t state;
    
    /* Timing tracking */
    uint64_t last_heartbeat_ms;  /* Timestamp of last heartbeat */
    uint8_t  has_prev_heartbeat; /* Have we seen at least one heartbeat? */
    
    /* Fault flags (aggregated from components) */
    uint8_t fault_pulse;         /* Pulse component faulted */
    uint8_t fault_baseline;      /* Baseline component faulted */
    
    /* Atomicity guard */
    uint8_t in_step;
    
    /* Statistics */
    uint32_t heartbeat_count;    /* Total heartbeats observed */
    uint32_t healthy_count;      /* Consecutive healthy observations */
    uint32_t unhealthy_count;    /* Consecutive unhealthy observations */
} timing_fsm_t;

/**
 * Result of a timing step.
 * 
 * Contains composed state plus component details for debugging.
 */
typedef struct {
    /* Composed state */
    timing_state_t state;
    
    /* Inter-arrival time (valid if has_dt is true) */
    double   dt;
    uint8_t  has_dt;
    
    /* Baseline z-score (valid if has_z is true) */
    double   z;
    uint8_t  has_z;
    
    /* Component states (for debugging/logging) */
    state_t      pulse_state;
    base_state_t baseline_state;
    
    /* Convenience flags */
    uint8_t is_healthy;    /* state == TIMING_HEALTHY */
    uint8_t is_unhealthy;  /* state == TIMING_UNHEALTHY */
    uint8_t is_dead;       /* state == TIMING_DEAD */
    uint8_t is_anomaly;    /* is_unhealthy || is_dead */
} timing_result_t;

/**
 * Initialise the timing monitor.
 * 
 * @param t   Pointer to timing FSM structure
 * @param cfg Configuration parameters (copied into t)
 * @return    0 on success, -1 on invalid parameters
 * 
 * CONTRACT: Configuration constraints must be satisfied:
 *   - heartbeat_timeout_ms > 0
 *   - init_window_ms >= 0
 *   - 0 < alpha < 1
 *   - epsilon > 0
 *   - k > 0
 *   - n_min >= ceil(2/alpha)
 * 
 * POSTCONDITION: t is in INITIALIZING state with zeroed statistics.
 */
int timing_init(timing_fsm_t *t, const timing_config_t *cfg);

/**
 * Process a heartbeat event.
 * 
 * This is the main entry point. Call when a heartbeat is observed.
 * 
 * @param t            Pointer to initialised timing FSM
 * @param timestamp_ms Current timestamp in milliseconds
 * @return             Result containing state and diagnostics
 * 
 * COMPOSITION LOGIC:
 *   1. Compute Δt from previous heartbeat (if any)
 *   2. Feed heartbeat to Pulse component
 *   3. If Δt available, feed to Baseline component
 *   4. Map component states to timing state
 *   5. Return composed result
 * 
 * GUARANTEE: Total function - always returns valid result.
 * GUARANTEE: Deterministic - same inputs → same outputs.
 */
timing_result_t timing_heartbeat(timing_fsm_t *t, uint64_t timestamp_ms);

/**
 * Check for timeout without a heartbeat event.
 * 
 * Call periodically to detect DEAD state when heartbeats stop.
 * 
 * @param t               Pointer to initialised timing FSM
 * @param current_time_ms Current timestamp in milliseconds
 * @return                Result containing state and diagnostics
 * 
 * NOTE: Does not generate Δt or update baseline statistics.
 *       Only checks if pulse has timed out.
 */
timing_result_t timing_check(timing_fsm_t *t, uint64_t current_time_ms);

/**
 * Reset timing monitor to initial state.
 * 
 * Clears all statistics and learned baselines.
 * Preserves configuration.
 * 
 * @param t Pointer to timing FSM
 * 
 * POSTCONDITION: state = INITIALIZING, statistics zeroed, faults cleared.
 */
void timing_reset(timing_fsm_t *t);

/**
 * Query current timing state.
 */
static inline timing_state_t timing_state(const timing_fsm_t *t) {
    return t->state;
}

/**
 * Check if any fault has been detected.
 */
static inline uint8_t timing_faulted(const timing_fsm_t *t) {
    return t->fault_pulse || t->fault_baseline;
}

/**
 * Check if timing is currently healthy.
 */
static inline uint8_t timing_healthy(const timing_fsm_t *t) {
    return t->state == TIMING_HEALTHY;
}

/**
 * Check if baseline has sufficient evidence.
 */
static inline uint8_t timing_ready(const timing_fsm_t *t) {
    return base_ready(&t->baseline);
}

/**
 * Get heartbeat count.
 */
static inline uint32_t timing_heartbeat_count(const timing_fsm_t *t) {
    return t->heartbeat_count;
}

/**
 * Convert state to string for display.
 */
static inline const char* timing_state_name(timing_state_t st) {
    switch (st) {
        case TIMING_INITIALIZING: return "INITIALIZING";
        case TIMING_HEALTHY:      return "HEALTHY";
        case TIMING_UNHEALTHY:    return "UNHEALTHY";
        case TIMING_DEAD:         return "DEAD";
        default:                  return "INVALID";
    }
}

#endif /* TIMING_H */
