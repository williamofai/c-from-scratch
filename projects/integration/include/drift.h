/**
 * drift.h - Rate & Trend Detection Monitor
 * 
 * A closed, total, deterministic state machine for detecting
 * dangerous rates of change in scalar observation streams.
 * 
 * Module 1 proved existence in time.
 * Module 2 proved normality in value.
 * Module 3 proved health over time.
 * Module 4 proves velocity toward failure.
 * 
 * THE CORE INSIGHT:
 *   "Temperature is normal now, but rising too fast."
 *   A signal within bounds but moving toward limits at dangerous velocity
 *   is a "silent failure" that absolute thresholds miss.
 * 
 * CONTRACTS:
 *   1. BOUNDED SLOPE:     |slope| bounded by physics, not runaway
 *   2. NOISE IMMUNITY:    Jitter < ε does not trigger DRIFTING
 *   3. TTF ACCURACY:      Time-to-failure estimate within bounded error
 *   4. SPIKE RESISTANCE:  Single outlier shifts slope by at most α·(outlier_slope)
 * 
 * REQUIREMENTS:
 *   - Single-writer access (caller must ensure)
 *   - Finite input values (no NaN/Inf)
 *   - Monotonic timestamps (caller provides)
 *   - Regular sampling rate (for meaningful slope)
 * 
 * See: lessons/ for proofs and data dictionary
 * 
 * Copyright (c) 2026 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#ifndef DRIFT_H
#define DRIFT_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/*===========================================================================
 * Fixed-Point Support (Q16.16) for FPU-less Hardware
 *===========================================================================*/

#ifdef DRIFT_FIXED_POINT
    typedef int32_t drift_value_t;
    #define DRIFT_VALUE_SCALE 65536
    #define DRIFT_TO_FLOAT(x)  ((double)(x) / DRIFT_VALUE_SCALE)
    #define DRIFT_TO_FIXED(x)  ((drift_value_t)((x) * DRIFT_VALUE_SCALE))
#else
    typedef double drift_value_t;
    #define DRIFT_VALUE_SCALE 1
    #define DRIFT_TO_FLOAT(x)  (x)
    #define DRIFT_TO_FIXED(x)  (x)
#endif

/*===========================================================================
 * Error Codes (Industry-Grade)
 *===========================================================================*/

typedef enum {
    DRIFT_OK           =  0,  /* Success */
    DRIFT_ERR_NULL     = -1,  /* NULL pointer passed */
    DRIFT_ERR_TEMPORAL = -2,  /* Timestamp out of order or gap too large */
    DRIFT_ERR_DOMAIN   = -3,  /* Input NaN, Inf, or out of range */
    DRIFT_ERR_OVERFLOW = -4,  /* Internal calculation overflow */
    DRIFT_ERR_STATE    = -5,  /* Module in invalid state */
    DRIFT_ERR_CONFIG   = -6,  /* Invalid configuration */
    DRIFT_ERR_FAULT    = -7   /* Hardware/sensor fault detected */
} drift_error_t;

/*===========================================================================
 * FSM States
 *===========================================================================*/

/**
 * Visible states of the drift monitor.
 * 
 * Zero-initialisation yields DRIFT_LEARNING (safe default).
 */
typedef enum {
    DRIFT_LEARNING     = 0,  /* Slope estimate not yet meaningful */
    DRIFT_STABLE       = 1,  /* Slope within safe bounds */
    DRIFT_DRIFTING_UP  = 2,  /* Slope exceeds positive threshold */
    DRIFT_DRIFTING_DOWN= 3,  /* Slope exceeds negative threshold */
    DRIFT_FAULT        = 4   /* Fault detected (NaN, overflow, etc.) */
} drift_state_t;

/*===========================================================================
 * Configuration
 *===========================================================================*/

/**
 * Configuration parameters (immutable after init).
 * 
 * CONSTRAINTS:
 *   C1: 0 < alpha <= 1.0         (EMA smoothing factor)
 *   C2: max_safe_slope > 0       (Threshold for DRIFTING)
 *   C3: upper_limit > lower_limit (Physical bounds for TTF)
 *   C4: n_min >= 2               (Minimum samples before STABLE)
 *   C5: max_gap > 0              (Maximum allowed Δt)
 *   C6: min_slope_for_ttf > 0    (Minimum slope magnitude for TTF calc)
 */
typedef struct {
    double   alpha;             /* EMA smoothing factor ∈ (0, 1]           */
    double   max_safe_slope;    /* Threshold for drift detection (units/ms)*/
    double   upper_limit;       /* Physical ceiling for TTF calculation    */
    double   lower_limit;       /* Physical floor for TTF calculation      */
    uint32_t n_min;             /* Minimum observations before STABLE      */
    uint64_t max_gap;           /* Maximum allowed Δt (ms) before reset    */
    double   min_slope_for_ttf; /* Min |slope| for meaningful TTF          */
    uint8_t  reset_on_gap;      /* true: auto-reset on gap, false: error   */
} drift_config_t;

/**
 * Default configuration.
 * 
 * alpha           = 0.1    Effective window ≈ 20 observations
 * max_safe_slope  = 0.1    Units per millisecond threshold
 * upper_limit     = 100.0  Physical ceiling
 * lower_limit     = 0.0    Physical floor
 * n_min           = 5      Learning period (need slope stability)
 * max_gap         = 5000   5 seconds max gap
 * min_slope_for_ttf= 1e-6  Minimum slope for TTF calculation
 * reset_on_gap    = 1      Auto-reset on large gaps
 */
static const drift_config_t DRIFT_DEFAULT_CONFIG = {
    .alpha           = 0.1,
    .max_safe_slope  = 0.1,
    .upper_limit     = 100.0,
    .lower_limit     = 0.0,
    .n_min           = 5,
    .max_gap         = 5000,
    .min_slope_for_ttf = 1e-6,
    .reset_on_gap    = 1
};

/*===========================================================================
 * FSM Structure
 *===========================================================================*/

/**
 * Drift Finite State Machine structure.
 * 
 * INVARIANTS:
 *   INV-1: state ∈ { LEARNING, STABLE, DRIFTING_UP, DRIFTING_DOWN, FAULT }
 *   INV-2: (state ≠ LEARNING) → (n >= cfg.n_min)
 *   INV-3: (fault_fp ∨ fault_reentry ∨ fault_overflow) → (state == FAULT)
 *   INV-4: (in_step == 0) when not executing drift_update
 *   INV-5: initialized == true after first valid update
 *   INV-6: (state == DRIFTING_UP) → (slope > cfg.max_safe_slope)
 *   INV-7: (state == DRIFTING_DOWN) → (slope < -cfg.max_safe_slope)
 *   INV-8: n increments monotonically on valid (non-faulted) input
 * 
 * FAULT BEHAVIOUR:
 *   fault_* flags are sticky; only cleared by drift_reset().
 *   Faulted input does NOT increment n.
 */
typedef struct {
    /* Configuration (immutable after init) */
    drift_config_t cfg;

    /* Minimal closed state: Sₜ = (slope, x_{t-1}, t_{t-1}, n, q) */
    double       slope;         /* Exponentially-weighted slope (damped derivative) */
    double       last_value;    /* Previous observation (x_{t-1}) */
    uint64_t     last_time;     /* Previous timestamp (t_{t-1}) */
    uint32_t     n;             /* Observation count */
    drift_state_t state;        /* FSM state (q) */

    /* Derived (computed per-step, not stored for closure) */
    double       ttf;           /* Time-to-failure estimate (last computed) */

    /* Initialization flag */
    uint8_t      initialized;   /* Have we seen at least one observation? */

    /* Fault flags (sticky until reset) */
    uint8_t      fault_fp;      /* NaN/Inf detected in input or state */
    uint8_t      fault_reentry; /* Atomicity violation detected */
    uint8_t      fault_overflow;/* Overflow detected in calculation */

    /* Atomicity guard */
    uint8_t      in_step;       /* Reentrancy guard */
} drift_fsm_t;

/*===========================================================================
 * Result Structure
 *===========================================================================*/

/**
 * Result of a single observation step.
 * 
 * Contains derived values that are NOT stored in FSM state.
 */
typedef struct {
    double       slope;         /* Current smoothed slope */
    double       raw_slope;     /* Instantaneous slope (before EMA) */
    double       ttf;           /* Time-to-failure estimate (ms) */
    double       dt;            /* Time delta from previous observation */
    drift_state_t state;        /* FSM state after this observation */
    uint8_t      is_drifting;   /* Convenience: DRIFTING_UP or DRIFTING_DOWN */
    uint8_t      has_ttf;       /* TTF is valid (not INFINITY) */
} drift_result_t;

/*===========================================================================
 * Public API
 *===========================================================================*/

/**
 * Initialise the drift FSM.
 * 
 * @param d   Pointer to FSM structure (must not be NULL)
 * @param cfg Configuration parameters (copied into d)
 * @return    DRIFT_OK on success, negative error code on failure
 *
 * CONTRACT: All config constraints (C1-C6) must be satisfied.
 * POSTCONDITION: d is in LEARNING state with zeroed statistics.
 * 
 * PRE:  d != NULL, cfg != NULL
 * PRE:  0 < cfg->alpha <= 1.0
 * PRE:  cfg->max_safe_slope > 0
 * PRE:  cfg->upper_limit > cfg->lower_limit
 * PRE:  cfg->n_min >= 2
 * PRE:  cfg->max_gap > 0
 * POST: d->state == DRIFT_LEARNING
 * POST: d->initialized == false
 * POST: d->n == 0
 */
int drift_init(drift_fsm_t *d, const drift_config_t *cfg);

/**
 * Execute one atomic step of the drift FSM.
 * 
 * This function is total: it always returns a valid result (via pointer).
 *
 * @param d         Pointer to initialised FSM
 * @param value     Observation value (must be finite)
 * @param timestamp Timestamp in milliseconds (must be monotonic)
 * @param result    Pointer to result structure (filled on return)
 * @return          DRIFT_OK on success, negative error code on failure
 *
 * CONTRACT: d must have been initialised via drift_init().
 * GUARANTEE: State transition follows FSM rules exactly.
 * GUARANTEE: If fault detected, state → FAULT, fault flag set, n unchanged.
 * 
 * UPDATE SEQUENCE (Damped Derivative):
 *   1. Validate input (finite check)
 *   2. Validate timestamp (monotonic, within gap limit)
 *   3. If first observation: store and return (no slope yet)
 *   4. Compute Δt = t - t_{t-1}
 *   5. Compute raw_slope = (x - x_{t-1}) / Δt
 *   6. Apply EMA: slope_t = α·raw_slope + (1-α)·slope_{t-1}
 *   7. Compute TTF if slope is significant
 *   8. Apply FSM transitions based on |slope|
 *   9. Update last_value, last_time, increment n
 * 
 * PRE:  d != NULL, result != NULL
 * PRE:  isfinite(value)
 * PRE:  timestamp > d->last_time (if initialized)
 * PRE:  (timestamp - d->last_time) <= cfg.max_gap (or auto-reset)
 * POST: d->n incremented (if no fault)
 * POST: result contains valid state and diagnostics
 */
int drift_update(drift_fsm_t *d, double value, uint64_t timestamp, 
                 drift_result_t *result);

/**
 * Reset drift to initial state (re-enter LEARNING).
 * Preserves configuration, clears statistics and faults.
 *
 * @param d Pointer to FSM
 *
 * PRE:  d != NULL (safe to call with NULL, becomes no-op)
 * POST: d->state == DRIFT_LEARNING
 * POST: d->initialized == false
 * POST: d->n == 0
 * POST: All fault flags cleared
 * POST: Configuration preserved
 */
void drift_reset(drift_fsm_t *d);

/*===========================================================================
 * Query Functions (Inline)
 *===========================================================================*/

/**
 * Query current FSM state.
 */
static inline drift_state_t drift_state(const drift_fsm_t *d) {
    return d->state;
}

/**
 * Check if any fault has been detected.
 */
static inline uint8_t drift_faulted(const drift_fsm_t *d) {
    return d->fault_fp || d->fault_reentry || d->fault_overflow;
}

/**
 * Check if drift is currently stable (not drifting, not faulted).
 */
static inline uint8_t drift_stable(const drift_fsm_t *d) {
    return d->state == DRIFT_STABLE;
}

/**
 * Check if drift is currently drifting (up or down).
 */
static inline uint8_t drift_is_drifting(const drift_fsm_t *d) {
    return d->state == DRIFT_DRIFTING_UP || d->state == DRIFT_DRIFTING_DOWN;
}

/**
 * Check if baseline has sufficient evidence.
 * 
 * Equivalent to: initialized && (n >= n_min)
 */
static inline uint8_t drift_ready(const drift_fsm_t *d) {
    return d->initialized && (d->n >= d->cfg.n_min);
}

/**
 * Get current slope estimate.
 */
static inline double drift_get_slope(const drift_fsm_t *d) {
    return d->slope;
}

/**
 * Get last computed TTF.
 */
static inline double drift_get_ttf(const drift_fsm_t *d) {
    return d->ttf;
}

/**
 * Convert state to string for display.
 */
static inline const char* drift_state_name(drift_state_t st) {
    switch (st) {
        case DRIFT_LEARNING:      return "LEARNING";
        case DRIFT_STABLE:        return "STABLE";
        case DRIFT_DRIFTING_UP:   return "DRIFTING_UP";
        case DRIFT_DRIFTING_DOWN: return "DRIFTING_DOWN";
        case DRIFT_FAULT:         return "FAULT";
        default:                  return "INVALID";
    }
}

/**
 * Convert error code to string for display.
 */
static inline const char* drift_error_name(drift_error_t err) {
    switch (err) {
        case DRIFT_OK:           return "OK";
        case DRIFT_ERR_NULL:     return "ERR_NULL";
        case DRIFT_ERR_TEMPORAL: return "ERR_TEMPORAL";
        case DRIFT_ERR_DOMAIN:   return "ERR_DOMAIN";
        case DRIFT_ERR_OVERFLOW: return "ERR_OVERFLOW";
        case DRIFT_ERR_STATE:    return "ERR_STATE";
        case DRIFT_ERR_CONFIG:   return "ERR_CONFIG";
        case DRIFT_ERR_FAULT:    return "ERR_FAULT";
        default:                 return "UNKNOWN";
    }
}

#endif /* DRIFT_H */
