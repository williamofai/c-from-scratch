/**
 * consensus.c - Triple Modular Redundancy Voter
 * 
 * Implementation of the consensus finite state machine.
 * 
 * This file is a direct transcription of the mathematical model
 * from Lesson 2. Every line traces to a contract or invariant.
 * 
 * THE VOTING ALGORITHM (Mid-Value Selection):
 *   Given inputs x₁, x₂, x₃:
 *   1. Filter to healthy sensors only
 *   2. Sort healthy values
 *   3. Select middle value (ignores extremes)
 * 
 * This guarantees CONTRACT-1: A single liar cannot corrupt output.
 * 
 * Copyright (c) 2026 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#include "consensus.h"
#include <string.h>
#include <math.h>

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * Check if a double is finite (not NaN or ±Inf).
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
 * Swap two doubles.
 */
static inline void swap_d(double *a, double *b)
{
    double tmp = *a;
    *a = *b;
    *b = tmp;
}

/**
 * Sort 3 values in place (simple bubble sort, O(1) for fixed n=3).
 */
static void sort3(double arr[3])
{
    if (arr[0] > arr[1]) swap_d(&arr[0], &arr[1]);
    if (arr[1] > arr[2]) swap_d(&arr[1], &arr[2]);
    if (arr[0] > arr[1]) swap_d(&arr[0], &arr[1]);
}

/**
 * Sort 2 values in place.
 */
static void sort2(double arr[2])
{
    if (arr[0] > arr[1]) swap_d(&arr[0], &arr[1]);
}

/*===========================================================================
 * Public API
 *===========================================================================*/

/**
 * Initialise the FSM.
 * 
 * Validates configuration and sets initial state.
 */
int consensus_init(consensus_fsm_t *c, const consensus_config_t *cfg)
{
    /* Null pointer checks */
    if (c == NULL || cfg == NULL) {
        return CONSENSUS_ERR_NULL;
    }

    /* C1: max_deviation > 0 */
    if (cfg->max_deviation <= 0.0) {
        return CONSENSUS_ERR_CONFIG;
    }

    /* C2: tie_breaker ∈ {0, 1, 2} */
    if (cfg->tie_breaker > 2) {
        return CONSENSUS_ERR_CONFIG;
    }

    /* Clear structure */
    memset(c, 0, sizeof(*c));

    /* Store configuration */
    c->cfg = *cfg;

    /* Initial state */
    c->state = CONSENSUS_INIT;
    c->n = 0;
    c->has_last = 0;
    c->last_value = 0.0;
    c->last_confidence = 0.0;

    return CONSENSUS_OK;
}

/**
 * Execute one atomic vote.
 * 
 * This is the core voting logic implementing Mid-Value Selection.
 */
int consensus_update(consensus_fsm_t *c,
                     const sensor_input_t inputs[CONSENSUS_NUM_SENSORS],
                     consensus_result_t *result)
{
    /* Initialize result to safe defaults */
    if (result != NULL) {
        result->value = 0.0;
        result->confidence = 0.0;
        result->state = CONSENSUS_FAULT;
        result->active_sensors = 0;
        result->sensors_agree = 0;
        result->spread = 0.0;
        result->valid = 0;
        for (int i = 0; i < CONSENSUS_NUM_SENSORS; i++) {
            result->used[i] = 0;
        }
    }

    /* Null pointer checks */
    if (c == NULL || inputs == NULL || result == NULL) {
        return CONSENSUS_ERR_NULL;
    }

    /*-----------------------------------------------------------------------
     * 1. Reentrancy Guard (INV-5)
     *-----------------------------------------------------------------------*/
    if (c->in_step) {
        c->fault_reentry = 1;
        c->state = CONSENSUS_FAULT;
        result->state = c->state;
        return CONSENSUS_ERR_REENTRY;
    }
    c->in_step = 1;

    /*-----------------------------------------------------------------------
     * 2. Already faulted? (sticky faults)
     *-----------------------------------------------------------------------*/
    if (consensus_faulted(c)) {
        result->state = c->state;
        c->in_step = 0;
        return CONSENSUS_ERR_FAULT;
    }

    /*-----------------------------------------------------------------------
     * 3. Validate inputs and count healthy sensors
     *-----------------------------------------------------------------------*/
    double healthy_values[CONSENSUS_NUM_SENSORS];
    int healthy_indices[CONSENSUS_NUM_SENSORS];
    int healthy_count = 0;

    for (int i = 0; i < CONSENSUS_NUM_SENSORS; i++) {
        /* Store for tracking */
        c->last_values[i] = inputs[i].value;
        c->last_health[i] = inputs[i].health;

        /* Check for NaN/Inf */
        if (!is_finite(inputs[i].value)) {
            /* Treat as faulty, don't hard-fault the module */
            continue;
        }

        /* Only include healthy or degraded sensors */
        if (inputs[i].health != SENSOR_FAULTY) {
            healthy_values[healthy_count] = inputs[i].value;
            healthy_indices[healthy_count] = i;
            result->used[i] = 1;
            healthy_count++;
        }
    }

    result->active_sensors = (uint8_t)healthy_count;

    /*-----------------------------------------------------------------------
     * 4. Check quorum (need at least 2 healthy sensors)
     *-----------------------------------------------------------------------*/
    if (healthy_count < 2) {
        c->state = CONSENSUS_NO_QUORUM;
        result->state = c->state;
        result->valid = 0;
        result->confidence = 0.0;

        /* Use last known value if available */
        if (c->has_last) {
            result->value = c->last_value;
            result->confidence = 0.1;  /* Very low confidence */
        }

        c->in_step = 0;
        return CONSENSUS_ERR_QUORUM;
    }

    /*-----------------------------------------------------------------------
     * 5. Voting Logic: Mid-Value Selection
     *-----------------------------------------------------------------------*/
    double consensus_value;
    double spread;

    if (healthy_count == 3) {
        /* TMR: Sort and take median (mid-value selection) */
        double sorted[3] = { healthy_values[0], healthy_values[1], healthy_values[2] };
        sort3(sorted);
        
        consensus_value = sorted[1];  /* Median */
        spread = sorted[2] - sorted[0];  /* Max - Min */
    }
    else if (healthy_count == 2) {
        /* Dual: Average or use tie-breaker */
        double sorted[2] = { healthy_values[0], healthy_values[1] };
        sort2(sorted);
        
        if (c->cfg.use_weighted_avg) {
            /* Simple average of two sensors */
            consensus_value = (healthy_values[0] + healthy_values[1]) / 2.0;
        } else {
            /* Use tie-breaker: prefer specific sensor if it's one of the healthy ones */
            int use_first = 1;
            for (int i = 0; i < 2; i++) {
                if (healthy_indices[i] == c->cfg.tie_breaker) {
                    consensus_value = healthy_values[i];
                    use_first = 0;
                    break;
                }
            }
            if (use_first) {
                /* Tie-breaker not in healthy set, use average */
                consensus_value = (healthy_values[0] + healthy_values[1]) / 2.0;
            }
        }
        
        spread = sorted[1] - sorted[0];
    }
    else {
        /* Should not reach here due to quorum check */
        c->in_step = 0;
        return CONSENSUS_ERR_FAULT;
    }

    result->value = consensus_value;
    result->spread = spread;

    /*-----------------------------------------------------------------------
     * 6. Compute agreement and confidence
     *-----------------------------------------------------------------------*/
    int sensors_agree = (spread <= c->cfg.max_deviation);
    result->sensors_agree = sensors_agree ? 1 : 0;

    /* Confidence calculation:
     *   - 3 sensors agree:    1.0
     *   - 3 sensors disagree: 0.7
     *   - 2 sensors agree:    0.8
     *   - 2 sensors disagree: 0.5
     */
    double confidence;
    if (healthy_count == 3) {
        confidence = sensors_agree ? 1.0 : 0.7;
    } else {  /* healthy_count == 2 */
        confidence = sensors_agree ? 0.8 : 0.5;
    }

    /* Reduce confidence if using degraded sensors */
    int degraded_count = 0;
    for (int i = 0; i < CONSENSUS_NUM_SENSORS; i++) {
        if (result->used[i] && inputs[i].health == SENSOR_DEGRADED) {
            degraded_count++;
        }
    }
    confidence -= degraded_count * 0.1;
    if (confidence < 0.1) confidence = 0.1;

    result->confidence = confidence;

    /*-----------------------------------------------------------------------
     * 7. FSM State Transitions
     *-----------------------------------------------------------------------*/
    c->n++;

    if (c->n >= c->cfg.n_min) {
        if (healthy_count == 3) {
            c->state = sensors_agree ? CONSENSUS_AGREE : CONSENSUS_DISAGREE;
        } else {  /* healthy_count == 2 */
            c->state = CONSENSUS_DEGRADED;
        }
    }

    result->state = c->state;
    result->valid = 1;

    /*-----------------------------------------------------------------------
     * 8. Store last known good values
     *-----------------------------------------------------------------------*/
    c->last_value = consensus_value;
    c->last_confidence = confidence;
    c->has_last = 1;

    c->in_step = 0;
    return CONSENSUS_OK;
}

/**
 * Convenience wrapper for array inputs.
 */
int consensus_update_arrays(consensus_fsm_t *c,
                            const double values[CONSENSUS_NUM_SENSORS],
                            const sensor_health_t health[CONSENSUS_NUM_SENSORS],
                            consensus_result_t *result)
{
    if (values == NULL || health == NULL) {
        return CONSENSUS_ERR_NULL;
    }

    sensor_input_t inputs[CONSENSUS_NUM_SENSORS];
    for (int i = 0; i < CONSENSUS_NUM_SENSORS; i++) {
        inputs[i].value = values[i];
        inputs[i].health = health[i];
    }

    return consensus_update(c, inputs, result);
}

/**
 * Reset to initial state.
 */
void consensus_reset(consensus_fsm_t *c)
{
    if (c == NULL) {
        return;
    }

    /* Preserve config */
    consensus_config_t cfg = c->cfg;

    /* Clear everything */
    memset(c, 0, sizeof(*c));

    /* Restore config */
    c->cfg = cfg;

    /* Set initial state */
    c->state = CONSENSUS_INIT;
}
