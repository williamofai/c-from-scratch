/**
 * drift.c - Rate & Trend Detection Monitor
 * 
 * Implementation of the drift finite state machine.
 * 
 * This file is a direct transcription of the mathematical model
 * from Lesson 2. Every line traces to a contract or invariant.
 * 
 * THE DAMPED DERIVATIVE:
 *   raw_slope = (xₜ - xₜ₋₁) / Δt
 *   slope_t = α · raw_slope + (1 - α) · slope_{t-1}
 * 
 * This is an EMA of the derivative — noise-immune slope tracking.
 * 
 * Copyright (c) 2026 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#include "drift.h"
#include <string.h>
#include <float.h>

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * Check if a double is finite (not NaN or ±Inf).
 * Required for fault detection (INV-3).
 */
static inline int is_finite(double x)
{
    return isfinite(x);
}

/**
 * Absolute value for doubles.
 */
static inline double abs_d(double x)
{
    return x < 0.0 ? -x : x;
}

/**
 * Safe division that checks for zero/near-zero denominator.
 * Returns INFINITY on division by zero.
 */
static inline double safe_divide(double num, double denom, double min_denom)
{
    if (abs_d(denom) < min_denom) {
        return (num >= 0) ? INFINITY : -INFINITY;
    }
    return num / denom;
}

/*===========================================================================
 * Public API
 *===========================================================================*/

/**
 * Initialise the FSM.
 * 
 * Validates configuration and sets initial state.
 * Returns DRIFT_OK on success, negative error code on failure.
 */
int drift_init(drift_fsm_t *d, const drift_config_t *cfg)
{
    /* Null pointer checks */
    if (d == NULL || cfg == NULL) {
        return DRIFT_ERR_NULL;
    }

    /* C1: 0 < alpha <= 1.0 */
    if (cfg->alpha <= 0.0 || cfg->alpha > 1.0) {
        return DRIFT_ERR_CONFIG;
    }

    /* C2: max_safe_slope > 0 */
    if (cfg->max_safe_slope <= 0.0) {
        return DRIFT_ERR_CONFIG;
    }

    /* C3: upper_limit > lower_limit */
    if (cfg->upper_limit <= cfg->lower_limit) {
        return DRIFT_ERR_CONFIG;
    }

    /* C4: n_min >= 2 */
    if (cfg->n_min < 2) {
        return DRIFT_ERR_CONFIG;
    }

    /* C5: max_gap > 0 */
    if (cfg->max_gap == 0) {
        return DRIFT_ERR_CONFIG;
    }

    /* C6: min_slope_for_ttf > 0 */
    if (cfg->min_slope_for_ttf <= 0.0) {
        return DRIFT_ERR_CONFIG;
    }

    /* Store configuration (immutable after init) */
    d->cfg = *cfg;

    /* Initialise state variables */
    d->slope = 0.0;
    d->last_value = 0.0;
    d->last_time = 0;
    d->n = 0;
    d->ttf = INFINITY;
    d->initialized = 0;

    /* Initial FSM state */
    d->state = DRIFT_LEARNING;

    /* Clear fault flags */
    d->fault_fp = 0;
    d->fault_reentry = 0;
    d->fault_overflow = 0;

    /* Clear atomicity guard */
    d->in_step = 0;

    return DRIFT_OK;
}

/**
 * Execute one atomic step of the FSM.
 * 
 * This function is TOTAL: it always populates result with valid data.
 * 
 * Structure:
 *   1. Reentrancy guard (INV-4)
 *   2. Already faulted check (sticky faults)
 *   3. Input validation (INV-3)
 *   4. Temporal validation (monotonic time-gate)
 *   5. First observation handling
 *   6. Time-gap protection
 *   7. Core state update (damped derivative)
 *   8. TTF calculation
 *   9. FSM transitions
 *   10. Build result
 */
int drift_update(drift_fsm_t *d, double value, uint64_t timestamp,
                 drift_result_t *result)
{
    /* Initialize result to safe defaults */
    if (result != NULL) {
        result->slope = 0.0;
        result->raw_slope = 0.0;
        result->ttf = INFINITY;
        result->dt = 0.0;
        result->state = DRIFT_FAULT;
        result->is_drifting = 0;
        result->has_ttf = 0;
    }

    /* Null pointer check */
    if (d == NULL) {
        return DRIFT_ERR_NULL;
    }

    if (result == NULL) {
        return DRIFT_ERR_NULL;
    }

    /*-----------------------------------------------------------------------
     * 1. Reentrancy Guard (INV-4)
     *-----------------------------------------------------------------------*/
    if (d->in_step) {
        d->fault_reentry = 1;
        d->state = DRIFT_FAULT;
        result->state = d->state;
        return DRIFT_ERR_FAULT;
    }
    d->in_step = 1;

    /*-----------------------------------------------------------------------
     * 2. Already faulted? (sticky faults)
     *-----------------------------------------------------------------------*/
    if (drift_faulted(d)) {
        result->state = d->state;
        d->in_step = 0;
        return DRIFT_ERR_FAULT;
    }

    /*-----------------------------------------------------------------------
     * 3. Input Validation (INV-3)
     *-----------------------------------------------------------------------*/
    if (!is_finite(value)) {
        d->fault_fp = 1;
        d->state = DRIFT_FAULT;
        result->state = d->state;
        d->in_step = 0;
        return DRIFT_ERR_DOMAIN;
    }

    /*-----------------------------------------------------------------------
     * 4. First Observation Handling
     * 
     * On first call, we only store the value and timestamp.
     * We cannot compute a slope without two points.
     *-----------------------------------------------------------------------*/
    if (!d->initialized) {
        d->last_value = value;
        d->last_time = timestamp;
        d->initialized = 1;
        d->n = 1;
        
        /* Still in LEARNING state */
        result->slope = 0.0;
        result->raw_slope = 0.0;
        result->ttf = INFINITY;
        result->dt = 0.0;
        result->state = d->state;
        result->is_drifting = 0;
        result->has_ttf = 0;
        
        d->in_step = 0;
        return DRIFT_OK;
    }

    /*-----------------------------------------------------------------------
     * 5. Temporal Validation (Monotonic Time-Gate)
     * 
     * TEMPORAL CONTRACT:
     *   PRE:  timestamp > d->last_time (strictly monotonic)
     *   POST: On violation, returns ERR_TEMPORAL
     *-----------------------------------------------------------------------*/
    if (timestamp <= d->last_time) {
        d->in_step = 0;
        return DRIFT_ERR_TEMPORAL;
    }

    /*-----------------------------------------------------------------------
     * 6. Time-Gap Protection
     * 
     * If Δt > max_gap, either auto-reset or return error.
     * This prevents stale EMA state from corrupting new data.
     *-----------------------------------------------------------------------*/
    uint64_t delta_t = timestamp - d->last_time;

    if (delta_t > d->cfg.max_gap) {
        if (d->cfg.reset_on_gap) {
            /* Auto-reset: preserve config, clear state */
            drift_config_t saved_cfg = d->cfg;
            d->slope = 0.0;
            d->last_value = value;
            d->last_time = timestamp;
            d->n = 1;
            d->ttf = INFINITY;
            d->state = DRIFT_LEARNING;
            d->cfg = saved_cfg;
            
            result->slope = 0.0;
            result->raw_slope = 0.0;
            result->ttf = INFINITY;
            result->dt = (double)delta_t;
            result->state = d->state;
            result->is_drifting = 0;
            result->has_ttf = 0;
            
            d->in_step = 0;
            return DRIFT_OK;  /* Fresh start */
        } else {
            d->in_step = 0;
            return DRIFT_ERR_TEMPORAL;  /* Caller decides */
        }
    }

    /*-----------------------------------------------------------------------
     * 7. Core State Update (Damped Derivative)
     * 
     * From Lesson 2:
     *   raw_slope = (xₜ - xₜ₋₁) / Δt
     *   slope_t = α · raw_slope + (1 - α) · slope_{t-1}
     *-----------------------------------------------------------------------*/
    double dt = (double)delta_t;
    double dx = value - d->last_value;
    
    /* Compute raw (instantaneous) slope */
    double raw_slope = dx / dt;
    
    /* Check for overflow in raw slope */
    if (!is_finite(raw_slope)) {
        d->fault_overflow = 1;
        d->state = DRIFT_FAULT;
        result->state = d->state;
        d->in_step = 0;
        return DRIFT_ERR_OVERFLOW;
    }
    
    /* Apply EMA (damped derivative) */
    double new_slope = (d->cfg.alpha * raw_slope) + 
                       ((1.0 - d->cfg.alpha) * d->slope);
    
    /* Check for overflow in EMA result */
    if (!is_finite(new_slope)) {
        d->fault_overflow = 1;
        d->state = DRIFT_FAULT;
        result->state = d->state;
        d->in_step = 0;
        return DRIFT_ERR_OVERFLOW;
    }
    
    d->slope = new_slope;

    /*-----------------------------------------------------------------------
     * 8. Time-To-Failure (TTF) Calculation
     * 
     * If slope is significant, estimate when we'll hit a limit:
     *   - If drifting up: TTF = (upper_limit - current) / slope
     *   - If drifting down: TTF = (current - lower_limit) / |slope|
     *-----------------------------------------------------------------------*/
    double ttf = INFINITY;
    uint8_t has_ttf = 0;

    if (d->slope > d->cfg.min_slope_for_ttf) {
        /* Drifting upward toward upper limit */
        double distance = d->cfg.upper_limit - value;
        if (distance > 0) {
            ttf = distance / d->slope;
            has_ttf = is_finite(ttf) && ttf > 0;
        }
    } else if (d->slope < -d->cfg.min_slope_for_ttf) {
        /* Drifting downward toward lower limit */
        double distance = value - d->cfg.lower_limit;
        if (distance > 0) {
            ttf = distance / abs_d(d->slope);
            has_ttf = is_finite(ttf) && ttf > 0;
        }
    }
    
    d->ttf = ttf;

    /*-----------------------------------------------------------------------
     * 9. Update tracking state
     *-----------------------------------------------------------------------*/
    d->last_value = value;
    d->last_time = timestamp;
    d->n++;

    /*-----------------------------------------------------------------------
     * 10. FSM Transitions (separate from math)
     * 
     * From Lesson 2 transition table:
     *   LEARNING → STABLE      when n >= n_min AND |slope| <= max_safe_slope
     *   LEARNING → DRIFTING_*  when n >= n_min AND |slope| > max_safe_slope
     *   STABLE → DRIFTING_UP   when slope > max_safe_slope
     *   STABLE → DRIFTING_DOWN when slope < -max_safe_slope
     *   DRIFTING_UP → STABLE   when slope <= max_safe_slope
     *   DRIFTING_DOWN → STABLE when slope >= -max_safe_slope
     *-----------------------------------------------------------------------*/
    double abs_slope = abs_d(d->slope);

    switch (d->state) {
        case DRIFT_LEARNING:
            if (d->n >= d->cfg.n_min) {
                /* Enough observations to make a judgment */
                if (abs_slope > d->cfg.max_safe_slope) {
                    d->state = (d->slope > 0) ? DRIFT_DRIFTING_UP : DRIFT_DRIFTING_DOWN;
                } else {
                    d->state = DRIFT_STABLE;
                }
            }
            break;

        case DRIFT_STABLE:
            if (d->slope > d->cfg.max_safe_slope) {
                d->state = DRIFT_DRIFTING_UP;
            } else if (d->slope < -d->cfg.max_safe_slope) {
                d->state = DRIFT_DRIFTING_DOWN;
            }
            break;

        case DRIFT_DRIFTING_UP:
            if (d->slope <= d->cfg.max_safe_slope) {
                d->state = DRIFT_STABLE;
            }
            break;

        case DRIFT_DRIFTING_DOWN:
            if (d->slope >= -d->cfg.max_safe_slope) {
                d->state = DRIFT_STABLE;
            }
            break;

        case DRIFT_FAULT:
            /* Stay in FAULT until reset */
            break;
    }

    /*-----------------------------------------------------------------------
     * 11. Build Result
     *-----------------------------------------------------------------------*/
    result->slope = d->slope;
    result->raw_slope = raw_slope;
    result->ttf = ttf;
    result->dt = dt;
    result->state = d->state;
    result->is_drifting = (d->state == DRIFT_DRIFTING_UP || 
                           d->state == DRIFT_DRIFTING_DOWN);
    result->has_ttf = has_ttf;

    d->in_step = 0;
    return DRIFT_OK;
}

/**
 * Reset to initial state.
 * 
 * Preserves configuration, clears state and faults.
 * This is the ONLY way to clear sticky faults.
 */
void drift_reset(drift_fsm_t *d)
{
    if (d == NULL) {
        return;
    }

    /* Preserve config */
    drift_config_t cfg = d->cfg;

    /* Clear everything */
    memset(d, 0, sizeof(*d));

    /* Restore config */
    d->cfg = cfg;

    /* Set initial state */
    d->state = DRIFT_LEARNING;
    d->ttf = INFINITY;
    d->initialized = 0;
}
