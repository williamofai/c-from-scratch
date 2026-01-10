/**
 * consensus.h - Triple Modular Redundancy Voter
 * 
 * A closed, total, deterministic state machine for achieving
 * consensus from multiple redundant sensor inputs.
 * 
 * Module 1 proved existence in time.
 * Module 2 proved normality in value.
 * Module 3 proved health over time.
 * Module 4 proved velocity toward failure.
 * Module 5 proves truth from many liars.
 * 
 * THE CORE INSIGHT:
 *   "A man with one clock knows what time it is.
 *    A man with two clocks is never sure."
 *   With THREE clocks, we can outvote the liar.
 * 
 * CONTRACTS:
 *   1. SINGLE-FAULT TOLERANCE: One faulty sensor does not corrupt output
 *   2. BOUNDED OUTPUT:         Consensus always within range of healthy inputs
 *   3. DETERMINISTIC:          Same inputs → Same consensus
 *   4. DEGRADATION AWARE:      Confidence decreases with fewer healthy sensors
 * 
 * REQUIREMENTS:
 *   - At least 2 of 3 sensors must be healthy for valid consensus
 *   - Single-writer access (caller must ensure)
 *   - Health states provided by upstream modules (Pulse/Baseline/Drift)
 * 
 * See: lessons/ for proofs and data dictionary
 * 
 * Copyright (c) 2026 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#ifndef CONSENSUS_H
#define CONSENSUS_H

#include <stdint.h>
#include <stdbool.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define CONSENSUS_NUM_SENSORS 3   /* TMR: Triple Modular Redundancy */

/*===========================================================================
 * Error Codes
 *===========================================================================*/

typedef enum {
    CONSENSUS_OK            =  0,  /* Success */
    CONSENSUS_ERR_NULL      = -1,  /* NULL pointer passed */
    CONSENSUS_ERR_CONFIG    = -2,  /* Invalid configuration */
    CONSENSUS_ERR_DOMAIN    = -3,  /* Input NaN or Inf */
    CONSENSUS_ERR_QUORUM    = -4,  /* Insufficient healthy sensors (<2) */
    CONSENSUS_ERR_FAULT     = -5,  /* Module in fault state */
    CONSENSUS_ERR_REENTRY   = -6   /* Reentrancy violation */
} consensus_error_t;

/*===========================================================================
 * Input Health States (from upstream modules)
 *===========================================================================*/

/**
 * Health state of an individual sensor/input channel.
 * 
 * These map to outputs from upstream modules:
 *   - HEALTHY: Pulse=ALIVE, Baseline=STABLE, Drift=STABLE
 *   - DEGRADED: Drift=DRIFTING (within limits but concerning)
 *   - FAULTY: Pulse=DEAD, Baseline=DEVIATION, or any FAULT state
 */
typedef enum {
    SENSOR_HEALTHY  = 0,  /* Fully operational */
    SENSOR_DEGRADED = 1,  /* Operational but concerning (e.g., drifting) */
    SENSOR_FAULTY   = 2   /* Not trustworthy, exclude from voting */
} sensor_health_t;

/*===========================================================================
 * FSM States
 *===========================================================================*/

/**
 * Consensus system states.
 * 
 * Zero-initialisation yields CONSENSUS_INIT (safe default).
 */
typedef enum {
    CONSENSUS_INIT      = 0,  /* Not yet received first input set */
    CONSENSUS_AGREE     = 1,  /* All healthy sensors agree (within tolerance) */
    CONSENSUS_DISAGREE  = 2,  /* Healthy sensors disagree beyond tolerance */
    CONSENSUS_DEGRADED  = 3,  /* Only 2 healthy sensors (still valid) */
    CONSENSUS_NO_QUORUM = 4,  /* <2 healthy sensors (no valid consensus) */
    CONSENSUS_FAULT     = 5   /* Internal fault detected */
} consensus_state_t;

/*===========================================================================
 * Configuration
 *===========================================================================*/

/**
 * Configuration parameters (immutable after init).
 * 
 * CONSTRAINTS:
 *   C1: max_deviation > 0        (Agreement tolerance)
 *   C2: tie_breaker ∈ {0, 1, 2}  (Sensor index for 2-sensor tie)
 *   C3: n_min >= 1               (Min updates before stable)
 */
typedef struct {
    double   max_deviation;    /* Max allowed spread for "agreement" */
    uint8_t  tie_breaker;      /* Which sensor wins ties (0, 1, or 2) */
    uint32_t n_min;            /* Minimum updates before AGREE state */
    uint8_t  use_weighted_avg; /* 0: mid-value selection, 1: weighted average */
} consensus_config_t;

/**
 * Default configuration.
 * 
 * max_deviation  = 1.0   Sensors must agree within ±1.0
 * tie_breaker    = 0     Sensor 0 wins ties
 * n_min          = 1     Immediately operational
 * use_weighted_avg = 0   Use mid-value selection (safer)
 */
static const consensus_config_t CONSENSUS_DEFAULT_CONFIG = {
    .max_deviation    = 1.0,
    .tie_breaker      = 0,
    .n_min            = 1,
    .use_weighted_avg = 0
};

/*===========================================================================
 * Input Structure
 *===========================================================================*/

/**
 * A single sensor input with its health state.
 */
typedef struct {
    double          value;   /* Sensor reading */
    sensor_health_t health;  /* Health from upstream module */
} sensor_input_t;

/*===========================================================================
 * Result Structure
 *===========================================================================*/

/**
 * Result of consensus voting.
 * 
 * Contains the voted value, confidence, and diagnostic information.
 */
typedef struct {
    /* Primary outputs */
    double           value;           /* Consensus value (voted result) */
    double           confidence;      /* 0.0 to 1.0 based on agreement */
    consensus_state_t state;          /* FSM state after this vote */
    
    /* Diagnostic information */
    uint8_t          active_sensors;  /* Count of healthy sensors (0-3) */
    uint8_t          sensors_agree;   /* All active sensors agree? */
    double           spread;          /* Max - Min of healthy inputs */
    
    /* Which sensors were used */
    uint8_t          used[CONSENSUS_NUM_SENSORS];  /* 1 if sensor contributed */
    
    /* Error state */
    uint8_t          valid;           /* 1 if consensus is valid */
} consensus_result_t;

/*===========================================================================
 * FSM Structure
 *===========================================================================*/

/**
 * Consensus Finite State Machine structure.
 * 
 * INVARIANTS:
 *   INV-1: state ∈ { INIT, AGREE, DISAGREE, DEGRADED, NO_QUORUM, FAULT }
 *   INV-2: (state == AGREE) → (active_sensors >= 2 ∧ spread <= max_deviation)
 *   INV-3: (state == NO_QUORUM) → (active_sensors < 2)
 *   INV-4: (fault_*) → (state == FAULT)
 *   INV-5: (in_step == 0) when not executing consensus_update
 * 
 * FAULT BEHAVIOUR:
 *   fault_* flags are sticky; only cleared by consensus_reset().
 */
typedef struct {
    /* Configuration (immutable after init) */
    consensus_config_t cfg;
    
    /* State */
    consensus_state_t state;
    uint32_t          n;              /* Update count */
    
    /* Last voted value (for continuity on partial failure) */
    double            last_value;
    double            last_confidence;
    uint8_t           has_last;
    
    /* Per-sensor tracking */
    double            last_values[CONSENSUS_NUM_SENSORS];
    sensor_health_t   last_health[CONSENSUS_NUM_SENSORS];
    
    /* Fault flags (sticky until reset) */
    uint8_t           fault_fp;       /* NaN/Inf detected */
    uint8_t           fault_reentry;  /* Atomicity violation */
    
    /* Atomicity guard */
    uint8_t           in_step;
} consensus_fsm_t;

/*===========================================================================
 * Public API
 *===========================================================================*/

/**
 * Initialise the consensus FSM.
 * 
 * @param c   Pointer to FSM structure (must not be NULL)
 * @param cfg Configuration parameters (copied into c)
 * @return    CONSENSUS_OK on success, negative error code on failure
 *
 * PRE:  c != NULL, cfg != NULL
 * PRE:  cfg->max_deviation > 0
 * PRE:  cfg->tie_breaker ∈ {0, 1, 2}
 * POST: c->state == CONSENSUS_INIT
 * POST: c->n == 0
 */
int consensus_init(consensus_fsm_t *c, const consensus_config_t *cfg);

/**
 * Execute one atomic vote with three sensor inputs.
 * 
 * This function is total: it always returns a valid result (via pointer).
 *
 * @param c       Pointer to initialised FSM
 * @param inputs  Array of 3 sensor inputs with health states
 * @param result  Pointer to result structure (filled on return)
 * @return        CONSENSUS_OK on success, negative error code on failure
 *
 * VOTING LOGIC (Mid-Value Selection):
 *   1. Filter to healthy sensors only
 *   2. If 0-1 healthy: NO_QUORUM, use last known value if available
 *   3. If 2 healthy: Use average (or tie_breaker if configured)
 *   4. If 3 healthy: Use median (mid-value)
 *   5. Compute spread and agreement
 *   6. Set confidence based on sensor count and agreement
 * 
 * PRE:  c != NULL, inputs != NULL, result != NULL
 * POST: result contains valid state and diagnostics
 * GUARANTEE: Single faulty sensor cannot corrupt output (CONTRACT-1)
 */
int consensus_update(consensus_fsm_t *c, 
                     const sensor_input_t inputs[CONSENSUS_NUM_SENSORS],
                     consensus_result_t *result);

/**
 * Convenience wrapper: update with raw values and health arrays.
 * 
 * @param c       Pointer to initialised FSM
 * @param values  Array of 3 sensor values
 * @param health  Array of 3 health states
 * @param result  Pointer to result structure
 * @return        CONSENSUS_OK on success, negative error code on failure
 */
int consensus_update_arrays(consensus_fsm_t *c,
                            const double values[CONSENSUS_NUM_SENSORS],
                            const sensor_health_t health[CONSENSUS_NUM_SENSORS],
                            consensus_result_t *result);

/**
 * Reset consensus to initial state.
 * Preserves configuration, clears state and faults.
 *
 * @param c Pointer to FSM
 *
 * PRE:  c != NULL (safe to call with NULL, becomes no-op)
 * POST: c->state == CONSENSUS_INIT
 * POST: c->n == 0
 * POST: All fault flags cleared
 */
void consensus_reset(consensus_fsm_t *c);

/*===========================================================================
 * Query Functions (Inline)
 *===========================================================================*/

/**
 * Query current FSM state.
 */
static inline consensus_state_t consensus_state(const consensus_fsm_t *c) {
    return c->state;
}

/**
 * Check if any fault has been detected.
 */
static inline uint8_t consensus_faulted(const consensus_fsm_t *c) {
    return c->fault_fp || c->fault_reentry;
}

/**
 * Check if consensus is in agreement.
 */
static inline uint8_t consensus_in_agreement(const consensus_fsm_t *c) {
    return c->state == CONSENSUS_AGREE;
}

/**
 * Check if consensus has quorum (>=2 healthy sensors).
 */
static inline uint8_t consensus_has_quorum(const consensus_fsm_t *c) {
    return c->state != CONSENSUS_NO_QUORUM && c->state != CONSENSUS_FAULT;
}

/**
 * Get last voted value.
 */
static inline double consensus_get_value(const consensus_fsm_t *c) {
    return c->last_value;
}

/**
 * Get last confidence.
 */
static inline double consensus_get_confidence(const consensus_fsm_t *c) {
    return c->last_confidence;
}

/**
 * Convert state to string for display.
 */
static inline const char* consensus_state_name(consensus_state_t st) {
    switch (st) {
        case CONSENSUS_INIT:      return "INIT";
        case CONSENSUS_AGREE:     return "AGREE";
        case CONSENSUS_DISAGREE:  return "DISAGREE";
        case CONSENSUS_DEGRADED:  return "DEGRADED";
        case CONSENSUS_NO_QUORUM: return "NO_QUORUM";
        case CONSENSUS_FAULT:     return "FAULT";
        default:                  return "INVALID";
    }
}

/**
 * Convert health state to string for display.
 */
static inline const char* sensor_health_name(sensor_health_t h) {
    switch (h) {
        case SENSOR_HEALTHY:  return "HEALTHY";
        case SENSOR_DEGRADED: return "DEGRADED";
        case SENSOR_FAULTY:   return "FAULTY";
        default:              return "INVALID";
    }
}

/**
 * Convert error code to string for display.
 */
static inline const char* consensus_error_name(consensus_error_t err) {
    switch (err) {
        case CONSENSUS_OK:         return "OK";
        case CONSENSUS_ERR_NULL:   return "ERR_NULL";
        case CONSENSUS_ERR_CONFIG: return "ERR_CONFIG";
        case CONSENSUS_ERR_DOMAIN: return "ERR_DOMAIN";
        case CONSENSUS_ERR_QUORUM: return "ERR_QUORUM";
        case CONSENSUS_ERR_FAULT:  return "ERR_FAULT";
        case CONSENSUS_ERR_REENTRY:return "ERR_REENTRY";
        default:                   return "UNKNOWN";
    }
}

#endif /* CONSENSUS_H */
