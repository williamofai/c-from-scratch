/**
 * baseline.c - Statistical Normality Monitor Implementation
 * 
 * This is a direct transcription of the mathematical model.
 * Every line traces to a contract, invariant, or transition rule.
 * 
 * See: lessons/module2-baseline/ for proofs
 * 
 * Copyright (c) 2025 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#include "baseline.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>

/*---------------------------------------------------------------------------
 * Helper Functions
 *---------------------------------------------------------------------------*/

/** Check if value is finite (not NaN, not Inf) */
static inline uint8_t is_finite(double x)
{
    return isfinite(x) != 0;
}

/** Absolute value (avoid dependency on fabs for teaching clarity) */
static inline double abs_d(double x)
{
    return (x < 0.0) ? -x : x;
}

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

int base_init(base_fsm_t *b, const base_config_t *cfg)
{
    /* Validate config constraints C1-C4 */
    if (b == NULL || cfg == NULL) {
        return -1;
    }
    
    /* C1: 0 < alpha < 1 */
    if (cfg->alpha <= 0.0 || cfg->alpha >= 1.0) {
        return -1;
    }
    
    /* C2: epsilon > 0 */
    if (cfg->epsilon <= 0.0) {
        return -1;
    }
    
    /* C3: k > 0 */
    if (cfg->k <= 0.0) {
        return -1;
    }
    
    /* C4: n_min >= ceil(2/alpha) — coupled to EMA memory */
    uint32_t min_required = (uint32_t)ceil(2.0 / cfg->alpha);
    if (cfg->n_min < min_required) {
        return -1;
    }
    
    /* Copy configuration (immutable after this point) */
    b->cfg = *cfg;
    
    /* Zero statistics */
    b->mu       = 0.0;
    b->variance = 0.0;
    b->sigma    = 0.0;
    b->n        = 0;
    
    /* Initial state */
    b->state = BASE_LEARNING;
    
    /* Clear faults */
    b->fault_fp      = 0;
    b->fault_reentry = 0;
    
    /* Clear atomicity guard */
    b->in_step = 0;
    
    return 0;
}

base_result_t base_step(base_fsm_t *b, double x)
{
    base_result_t result = {0};
    
    /*-----------------------------------------------------------------------
     * Reentrancy check — CONTRACT enforcement (INV-4)
     *-----------------------------------------------------------------------*/
    if (b->in_step) {
        b->fault_reentry = 1;
        b->state = BASE_DEVIATION;  /* INV-3: fault → DEVIATION */
        result.state = b->state;
        result.is_deviation = 1;
        return result;
    }
    b->in_step = 1;
    
    /*-----------------------------------------------------------------------
     * Input validation — fault_fp on NaN/Inf
     * 
     * NOTE: Faults are checked regardless of FSM state. Being in LEARNING
     * does not protect against faults. A faulted system cannot certify
     * normality, so state → DEVIATION even from LEARNING.
     *-----------------------------------------------------------------------*/
    if (!is_finite(x)) {
        b->fault_fp = 1;
        b->state = BASE_DEVIATION;  /* INV-3: fault → DEVIATION */
        /* n unchanged on fault (INV-7) */
        result.state = b->state;
        result.is_deviation = 1;
        b->in_step = 0;
        return result;
    }
    
    /*-----------------------------------------------------------------------
     * Statistics update — direct from mathematical design
     * 
     * UPDATE SEQUENCE:
     *   1. deviation = xₜ - μₜ₋₁           (using mean BEFORE update)
     *   2. μₜ = α·xₜ + (1-α)·μₜ₋₁         (update mean)
     *   3. σₜ² = α·deviation² + (1-α)·σₜ₋₁² (update variance)
     *   4. σₜ = √σₜ²                       (update sigma)
     *   5. z = |deviation| / σₜ            (using sigma AFTER update)
     *-----------------------------------------------------------------------*/
    
    double mu_old = b->mu;
    double alpha  = b->cfg.alpha;
    
    /* Step 1: deviation using μₜ₋₁ */
    double deviation = x - mu_old;
    
    /* Step 2: update mean */
    double mu_new = alpha * x + (1.0 - alpha) * mu_old;
    
    /* Step 3: update variance */
    double var_new = alpha * (deviation * deviation) + (1.0 - alpha) * b->variance;
    
    /* Step 4: update sigma (cached √variance) */
    double sigma_new = sqrt(var_new);
    
    /* Check for numerical fault in computed values */
    if (!is_finite(mu_new) || !is_finite(var_new) || !is_finite(sigma_new)) {
        b->fault_fp = 1;
        b->state = BASE_DEVIATION;
        result.state = b->state;
        result.is_deviation = 1;
        b->in_step = 0;
        return result;
    }
    
    /* Commit state updates */
    b->mu       = mu_new;
    b->variance = var_new;
    b->sigma    = sigma_new;
    b->n       += 1;  /* INV-7: monotonic increment on success */
    
    /* Step 5: compute z-score */
    double z;
    if (b->variance <= b->cfg.epsilon) {
        /* Variance floor: cannot compute meaningful z-score */
        z = 0.0;
    } else {
        z = abs_d(deviation) / sigma_new;
    }
    
    /* Populate result (derived values, not stored) */
    result.deviation = deviation;
    result.z         = z;
    
    /*-----------------------------------------------------------------------
     * FSM Transitions — direct from transition rules
     *-----------------------------------------------------------------------*/
    
    switch (b->state) {
        case BASE_LEARNING:
            /* LEARNING → STABLE when base_ready(b) becomes true */
            if ((b->n >= b->cfg.n_min) && (b->variance > b->cfg.epsilon)) {
                b->state = BASE_STABLE;
            }
            /* else: remain in LEARNING */
            break;
            
        case BASE_STABLE:
            /* STABLE → DEVIATION when z > k */
            /* STABLE → STABLE    when z ≤ k */
            if (z > b->cfg.k) {
                b->state = BASE_DEVIATION;
            }
            break;
            
        case BASE_DEVIATION:
            /* DEVIATION → STABLE    when z ≤ k (and not faulted) */
            /* DEVIATION → DEVIATION when z > k */
            if (!base_faulted(b) && z <= b->cfg.k) {
                b->state = BASE_STABLE;
            }
            break;
            
        default:
            /* Invalid state — treat as fault */
            b->fault_fp = 1;
            b->state = BASE_DEVIATION;
            break;
    }
    
    result.state = b->state;
    result.is_deviation = (b->state == BASE_DEVIATION) ? 1 : 0;
    
    b->in_step = 0;
    return result;
}

void base_reset(base_fsm_t *b)
{
    /* Preserve configuration */
    /* Clear statistics */
    b->mu       = 0.0;
    b->variance = 0.0;
    b->sigma    = 0.0;
    b->n        = 0;
    
    /* Reset state */
    b->state = BASE_LEARNING;
    
    /* Clear faults (sticky until reset) */
    b->fault_fp      = 0;
    b->fault_reentry = 0;
    
    /* Clear atomicity guard */
    b->in_step = 0;
}
