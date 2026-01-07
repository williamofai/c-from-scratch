/**
 * baseline.h - Statistical Normality Monitor
 * 
 * A closed, total, deterministic state machine for detecting
 * statistical deviations in scalar observation streams.
 * 
 * Module 1 proved existence in time.
 * Module 2 proves normality in value.
 * 
 * CONTRACTS:
 *   1. CONVERGENCE:  μₜ → E[X] for stationary input
 *   2. SENSITIVITY:  Deviation >kσ detected in O(1/α) observations
 *   3. STABILITY:    False positive rate bounded by P(|Z|>k)
 *   4. SPIKE RESISTANCE: Single outlier M shifts mean by at most α·M
 * 
 * REQUIREMENTS:
 *   - Single-writer access (caller must ensure)
 *   - Finite input values (no NaN/Inf)
 *   - Regular observation rate (caller provides)
 * 
 * See: lessons/module2-baseline/ for proofs and data dictionary
 * 
 * Copyright (c) 2025 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#ifndef BASELINE_H
#define BASELINE_H

#include <stdint.h>

/**
 * Visible states of the normality monitor.
 * 
 * Zero-initialisation yields BASE_LEARNING (safe default).
 */
typedef enum {
    BASE_LEARNING  = 0,  /* Statistics not yet meaningful        */
    BASE_STABLE    = 1,  /* Baseline established, operating normally */
    BASE_DEVIATION = 2   /* Anomaly detected or fault occurred   */
} base_state_t;

/**
 * Configuration parameters (immutable after init).
 * 
 * CONSTRAINTS:
 *   C1: 0 < alpha < 1
 *   C2: epsilon > 0
 *   C3: k > 0
 *   C4: n_min >= ceil(2/alpha)
 */
typedef struct {
    double   alpha;      /* EMA smoothing factor ∈ (0, 1)              */
    double   epsilon;    /* Variance floor for safe z-score computation */
    double   k;          /* Deviation threshold (z-score units)        */
    uint32_t n_min;      /* Minimum observations before STABLE         */
} base_config_t;

/**
 * Baseline Finite State Machine structure.
 * 
 * INVARIANTS:
 *   INV-1: state ∈ { LEARNING, STABLE, DEVIATION }
 *   INV-2: (state ≠ LEARNING) → (n ≥ cfg.n_min ∧ variance > cfg.epsilon)
 *   INV-3: (fault_fp ∨ fault_reentry) → (state == DEVIATION)
 *   INV-4: (in_step == 0) when not executing base_step
 *   INV-5: variance ≥ 0
 *   INV-6: sigma == √variance (cached, always consistent)
 *   INV-7: n increments monotonically (nₜ = nₜ₋₁ + 1 on each non-faulted step)
 * 
 * FAULT BEHAVIOUR:
 *   fault_* flags are sticky; only cleared by base_reset().
 *   Faulted input does NOT increment n.
 */
typedef struct {
    /* Configuration (immutable after init) */
    base_config_t cfg;

    /* Minimal closed state: Sₜ = (μₜ, σₜ², nₜ, qₜ) */
    double       mu;            /* Exponentially-weighted mean (μₜ)     */
    double       variance;      /* Exponentially-weighted variance (σₜ²)*/
    double       sigma;         /* Cached: √variance (σₜ)               */
    uint32_t     n;             /* Observation count (nₜ)               */
    base_state_t state;         /* FSM state (qₜ)                       */

    /* Fault flags (sticky until reset) */
    uint8_t      fault_fp;      /* NaN/Inf detected in input or state   */
    uint8_t      fault_reentry; /* Atomicity violation detected         */

    /* Atomicity guard */
    uint8_t      in_step;       /* Reentrancy guard                     */
} base_fsm_t;

/**
 * Result of a single observation step.
 * 
 * Contains derived values that are NOT stored in FSM state.
 */
typedef struct {
    double       z;             /* Computed z-score: |deviation| / σₜ   */
    double       deviation;     /* Raw deviation: (xₜ - μₜ₋₁)           */
    base_state_t state;         /* FSM state after this observation     */
    uint8_t      is_deviation;  /* Convenience: state == BASE_DEVIATION */
} base_result_t;

/**
 * Default configuration.
 * 
 * alpha   = 0.1   Effective window ≈ 20 observations
 * epsilon = 1e-9  Variance floor for numerical safety
 * k       = 3.0   Three-sigma deviation threshold
 * n_min   = 20    ceil(2/alpha) = 20 (EMA warm-up)
 */
static const base_config_t BASE_DEFAULT_CONFIG = {
    .alpha   = 0.1,
    .epsilon = 1e-9,
    .k       = 3.0,
    .n_min   = 20
};

/**
 * Initialise the baseline FSM.
 * 
 * @param b   Pointer to FSM structure (must not be NULL)
 * @param cfg Configuration parameters (copied into b)
 * @return    0 on success, -1 on invalid parameters
 *
 * CONTRACT: All config constraints (C1-C4) must be satisfied.
 * POSTCONDITION: b is in LEARNING state with zeroed statistics.
 */
int base_init(base_fsm_t *b, const base_config_t *cfg);

/**
 * Execute one atomic step of the baseline FSM.
 * 
 * This function is total: it always returns a valid base_result_t.
 *
 * @param b Pointer to initialised FSM
 * @param x Observation value (must be finite)
 * @return  Result containing z-score and new state
 *
 * CONTRACT: b must have been initialised via base_init().
 * GUARANTEE: State transition follows FSM rules exactly.
 * GUARANTEE: If fault detected, state → DEVIATION, fault flag set, n unchanged.
 * 
 * UPDATE SEQUENCE:
 *   1. deviation = xₜ - μₜ₋₁           (using mean BEFORE update)
 *   2. μₜ = α·xₜ + (1-α)·μₜ₋₁         (update mean)
 *   3. σₜ² = α·deviation² + (1-α)·σₜ₋₁² (update variance)
 *   4. σₜ = √σₜ²                       (update sigma)
 *   5. z = |deviation| / σₜ            (using sigma AFTER update)
 */
base_result_t base_step(base_fsm_t *b, double x);

/**
 * Reset baseline to initial state (re-enter LEARNING).
 * Preserves configuration, clears statistics and faults.
 *
 * @param b Pointer to FSM
 *
 * POSTCONDITION: Statistics zeroed, state = LEARNING, faults cleared.
 */
void base_reset(base_fsm_t *b);

/**
 * Query current FSM state.
 */
static inline base_state_t base_state(const base_fsm_t *b) {
    return b->state;
}

/**
 * Check if any fault has been detected.
 */
static inline uint8_t base_faulted(const base_fsm_t *b) {
    return b->fault_fp || b->fault_reentry;
}

/**
 * Check if baseline is ready (has sufficient evidence).
 * 
 * Equivalent to: (n >= n_min) && (variance > epsilon)
 * 
 * NOTE: base_ready(b) implies state != BASE_LEARNING (by INV-2)
 */
static inline uint8_t base_ready(const base_fsm_t *b) {
    return (b->n >= b->cfg.n_min) && (b->variance > b->cfg.epsilon);
}

/**
 * Convert state to string for display.
 */
static inline const char* base_state_name(base_state_t st) {
    switch (st) {
        case BASE_LEARNING:  return "LEARNING";
        case BASE_STABLE:    return "STABLE";
        case BASE_DEVIATION: return "DEVIATION";
        default:             return "INVALID";
    }
}

#endif /* BASELINE_H */
