/**
 * mode.h - System Mode Manager (Module 7)
 * 
 * The "Captain" of the safety-critical ship.
 * Composes health signals from Modules 1-6 into a formal,
 * deterministic Hierarchical State Machine.
 * 
 * While Modules 1-6 answer "What's happening?",
 * Module 7 answers "What do we DO about it?"
 * 
 * CONTRACTS:
 *   1. UNAMBIGUOUS STATE:   System exists in exactly one mode at any time
 *   2. SAFE ENTRY:          OPERATIONAL requires all monitors HEALTHY
 *   3. FAULT STICKINESS:    EMERGENCY requires explicit reset to exit
 *   4. NO SKIP:             Transitions must follow valid paths
 *   5. BOUNDED LATENCY:     Fault → EMERGENCY in ≤ 1 cycle
 *   6. DETERMINISTIC:       Same inputs → Same mode
 *   7. PROACTIVE SAFETY:    Value flags trigger DEGRADED before faults
 *   8. AUDITABILITY:        All transitions logged with cause
 * 
 * INVARIANTS:
 *   INV-1: mode ∈ { INIT, STARTUP, OPERATIONAL, DEGRADED, EMERGENCY, TEST }
 *   INV-2: (mode == OPERATIONAL) → all states HEALTHY AND no critical flags
 *   INV-3: (mode == EMERGENCY) → (fault_active == true)
 *   INV-4: ticks_in_mode increments monotonically until transition
 *   INV-5: history_count ≤ history_capacity
 * 
 * Copyright (c) 2026 William Murray
 * MIT License
 */

#ifndef MODE_H
#define MODE_H

#include <stdint.h>
#include <stdbool.h>

/*===========================================================================
 * Configuration
 *===========================================================================*/

#define MODE_HISTORY_SIZE   16      /* Circular buffer of transitions */
#define MODE_MODULE_COUNT    6      /* Number of foundation modules */

/*===========================================================================
 * Error Codes
 *===========================================================================*/

typedef enum {
    MODE_OK           =  0,
    MODE_ERR_NULL     = -1,
    MODE_ERR_CONFIG   = -2,
    MODE_ERR_STATE    = -3,
    MODE_ERR_LOCKED   = -4     /* Cannot transition from EMERGENCY */
} mode_error_t;

/*===========================================================================
 * System Modes
 *===========================================================================*/

/**
 * High-level operational modes.
 * 
 * INIT        - Power-on, hardware check, safety validation
 * STARTUP     - Learning period for statistical modules
 * OPERATIONAL - Full system functionality allowed
 * DEGRADED    - Reduced functionality, approaching limits
 * EMERGENCY   - Critical fault, safe-state shutdown (sticky)
 * TEST        - Maintenance/diagnostic mode (bypasses some checks)
 */
typedef enum {
    MODE_INIT        = 0,
    MODE_STARTUP     = 1,
    MODE_OPERATIONAL = 2,
    MODE_DEGRADED    = 3,
    MODE_EMERGENCY   = 4,
    MODE_TEST        = 5,
    MODE_COUNT       = 6
} system_mode_t;

/*===========================================================================
 * Module States (Normalized)
 *===========================================================================*/

/**
 * Normalized health state from any module.
 * Each foundation module maps its internal state to one of these.
 */
typedef enum {
    HEALTH_UNKNOWN   = 0,   /* Not yet initialized */
    HEALTH_LEARNING  = 1,   /* Gathering baseline data */
    HEALTH_HEALTHY   = 2,   /* Normal operation */
    HEALTH_DEGRADED  = 3,   /* Warning, still functional */
    HEALTH_FAULTY    = 4    /* Critical failure */
} health_state_t;

/*===========================================================================
 * Value-Awareness Flags
 *===========================================================================*/

/**
 * Semantic flags set by modules based on domain knowledge.
 * These enable proactive safety — act before failure.
 * 
 * Modules decide their own thresholds:
 *   - Drift sets approaching_upper when TTF < critical_window
 *   - Consensus sets low_confidence when confidence < 50%
 *   - Pressure sets queue_critical when fill > 90%
 */
typedef struct {
    uint8_t approaching_upper : 1;  /* Drift: value approaching upper limit */
    uint8_t approaching_lower : 1;  /* Drift: value approaching lower limit */
    uint8_t low_confidence    : 1;  /* Consensus: confidence below threshold */
    uint8_t queue_critical    : 1;  /* Pressure: queue nearly full */
    uint8_t timing_unstable   : 1;  /* Timing: recent jitter violations */
    uint8_t baseline_volatile : 1;  /* Baseline: high recent deviation */
    uint8_t reserved          : 2;
} mode_flags_t;

/*===========================================================================
 * Mode Input Structure
 *===========================================================================*/

/**
 * Input to the Mode Manager from all foundation modules.
 */
typedef struct {
    health_state_t states[MODE_MODULE_COUNT];   /* State from each module */
    mode_flags_t flags;                          /* Semantic warning flags */
    uint64_t timestamp;                          /* Current time (ms) */
} mode_input_t;

/* Module indices for the states array */
typedef enum {
    MOD_PULSE     = 0,
    MOD_BASELINE  = 1,
    MOD_TIMING    = 2,
    MOD_DRIFT     = 3,
    MOD_CONSENSUS = 4,
    MOD_PRESSURE  = 5
} module_index_t;

/*===========================================================================
 * Mode Permissions
 *===========================================================================*/

/**
 * What actions are allowed in each mode.
 * This is the key safety mechanism — modes constrain behaviour.
 */
typedef struct {
    uint8_t can_actuate     : 1;   /* Fire thrusters, start motors */
    uint8_t can_calibrate   : 1;   /* Run calibration routines */
    uint8_t can_log         : 1;   /* Write to storage */
    uint8_t can_communicate : 1;   /* Send telemetry */
    uint8_t reserved        : 4;
} mode_permissions_t;

/*===========================================================================
 * Transition History (Audit Log)
 *===========================================================================*/

/**
 * Record of a mode transition for audit trail.
 */
typedef struct {
    uint64_t timestamp;         /* When the transition occurred */
    system_mode_t from_mode;    /* Previous mode */
    system_mode_t to_mode;      /* New mode */
    uint8_t trigger_mask;       /* Bitmask: which modules contributed */
    uint8_t flags_snapshot;     /* Warning flags at time of transition */
} mode_transition_t;

/* Trigger mask bits */
#define TRIGGER_PULSE       (1 << 0)
#define TRIGGER_BASELINE    (1 << 1)
#define TRIGGER_TIMING      (1 << 2)
#define TRIGGER_DRIFT       (1 << 3)
#define TRIGGER_CONSENSUS   (1 << 4)
#define TRIGGER_PRESSURE    (1 << 5)
#define TRIGGER_FLAGS       (1 << 6)   /* Value-awareness flags */
#define TRIGGER_RESET       (1 << 7)   /* Manual reset */

/*===========================================================================
 * Configuration
 *===========================================================================*/

/**
 * Mode Manager configuration.
 * 
 * CONSTRAINTS:
 *   C1: min_dwell_startup >= 1
 *   C2: min_dwell_degraded >= 1
 */
typedef struct {
    uint32_t min_dwell_startup;     /* Min cycles in STARTUP before OPERATIONAL */
    uint32_t min_dwell_degraded;    /* Min cycles in DEGRADED before recovery */
    uint8_t use_value_flags;        /* 0 = state-only, 1 = state + flags */
    uint8_t require_all_healthy;    /* 1 = all modules must be HEALTHY for OPERATIONAL */
} mode_config_t;

/**
 * Default configuration.
 */
static const mode_config_t MODE_DEFAULT_CONFIG = {
    .min_dwell_startup = 10,
    .min_dwell_degraded = 5,
    .use_value_flags = 1,
    .require_all_healthy = 1
};

/*===========================================================================
 * Mode Result
 *===========================================================================*/

/**
 * Result of a mode update.
 */
typedef struct {
    system_mode_t mode;             /* Current mode */
    mode_permissions_t permissions; /* What's allowed */
    uint32_t ticks_in_mode;         /* How long in current mode */
    uint8_t transitioned;           /* 1 if mode changed this cycle */
    uint8_t fault_active;           /* 1 if in fault state */
} mode_result_t;

/*===========================================================================
 * Mode Manager FSM
 *===========================================================================*/

/**
 * Mode Manager state machine.
 */
typedef struct {
    /* Configuration (immutable after init) */
    mode_config_t cfg;
    
    /* Current state */
    system_mode_t mode;
    uint32_t ticks_in_mode;
    uint8_t fault_active;
    
    /* Transition history (circular buffer) */
    mode_transition_t history[MODE_HISTORY_SIZE];
    uint8_t history_head;
    uint8_t history_count;
    
    /* Statistics */
    uint32_t total_transitions;
    uint32_t emergency_count;
    
    /* Validity */
    uint8_t initialized;
} mode_manager_t;

/*===========================================================================
 * Permissions Table
 *===========================================================================*/

/**
 * What actions are permitted in each mode.
 * 
 * INIT:        Log and communicate only (safety checks)
 * STARTUP:     Add calibration
 * OPERATIONAL: Full functionality
 * DEGRADED:    Log and communicate only (conservative)
 * EMERGENCY:   Log and communicate only (minimal)
 * TEST:        Full functionality (maintenance override)
 */
static const mode_permissions_t MODE_PERMISSIONS[MODE_COUNT] = {
    [MODE_INIT]        = { .can_actuate = 0, .can_calibrate = 0, .can_log = 1, .can_communicate = 1 },
    [MODE_STARTUP]     = { .can_actuate = 0, .can_calibrate = 1, .can_log = 1, .can_communicate = 1 },
    [MODE_OPERATIONAL] = { .can_actuate = 1, .can_calibrate = 1, .can_log = 1, .can_communicate = 1 },
    [MODE_DEGRADED]    = { .can_actuate = 0, .can_calibrate = 0, .can_log = 1, .can_communicate = 1 },
    [MODE_EMERGENCY]   = { .can_actuate = 0, .can_calibrate = 0, .can_log = 1, .can_communicate = 1 },
    [MODE_TEST]        = { .can_actuate = 1, .can_calibrate = 1, .can_log = 1, .can_communicate = 1 }
};

/*===========================================================================
 * Public API
 *===========================================================================*/

/**
 * Initialise the Mode Manager.
 * 
 * @param m     Mode manager instance
 * @param cfg   Configuration (NULL for defaults)
 * @return      MODE_OK or error code
 */
int mode_init(mode_manager_t *m, const mode_config_t *cfg);

/**
 * Step the Mode FSM based on aggregate health input.
 * 
 * This is the primary entry point for system logic.
 * Call once per control cycle with current module states.
 * 
 * @param m      Mode manager instance
 * @param input  Health states and flags from all modules
 * @param result Output: current mode and permissions
 * @return       MODE_OK or error code
 */
int mode_update(mode_manager_t *m, const mode_input_t *input, mode_result_t *result);

/**
 * Force a reset to INIT mode.
 * Required for exiting sticky EMERGENCY mode.
 * 
 * @param m     Mode manager instance
 */
void mode_reset(mode_manager_t *m);

/**
 * Get transition history.
 * 
 * @param m         Mode manager instance
 * @param out       Array to fill with transitions
 * @param max_count Maximum entries to return
 * @return          Number of entries written
 */
int mode_get_history(const mode_manager_t *m, mode_transition_t *out, int max_count);

/**
 * Enter TEST mode (maintenance override).
 * 
 * @param m     Mode manager instance
 * @return      MODE_OK or MODE_ERR_LOCKED if in EMERGENCY
 */
int mode_enter_test(mode_manager_t *m);

/**
 * Exit TEST mode, return to INIT.
 * 
 * @param m     Mode manager instance
 */
void mode_exit_test(mode_manager_t *m);

/*===========================================================================
 * Inline Queries
 *===========================================================================*/

/**
 * Get current system mode.
 */
static inline system_mode_t mode_get(const mode_manager_t *m) {
    return m ? m->mode : MODE_INIT;
}

/**
 * Get permissions for current mode.
 */
static inline mode_permissions_t mode_permissions(const mode_manager_t *m) {
    if (!m) return (mode_permissions_t){0};
    return MODE_PERMISSIONS[m->mode];
}

/**
 * Check if actuation is allowed.
 */
static inline int mode_can_actuate(const mode_manager_t *m) {
    return m ? MODE_PERMISSIONS[m->mode].can_actuate : 0;
}

/**
 * Check if in fault state.
 */
static inline int mode_is_fault(const mode_manager_t *m) {
    return m ? m->fault_active : 0;
}

/**
 * Get time in current mode.
 */
static inline uint32_t mode_dwell_time(const mode_manager_t *m) {
    return m ? m->ticks_in_mode : 0;
}

/*===========================================================================
 * String Conversion
 *===========================================================================*/

static inline const char* mode_name(system_mode_t mode) {
    switch (mode) {
        case MODE_INIT:        return "INIT";
        case MODE_STARTUP:     return "STARTUP";
        case MODE_OPERATIONAL: return "OPERATIONAL";
        case MODE_DEGRADED:    return "DEGRADED";
        case MODE_EMERGENCY:   return "EMERGENCY";
        case MODE_TEST:        return "TEST";
        default:               return "UNKNOWN";
    }
}

static inline const char* health_name(health_state_t h) {
    switch (h) {
        case HEALTH_UNKNOWN:  return "UNKNOWN";
        case HEALTH_LEARNING: return "LEARNING";
        case HEALTH_HEALTHY:  return "HEALTHY";
        case HEALTH_DEGRADED: return "DEGRADED";
        case HEALTH_FAULTY:   return "FAULTY";
        default:              return "UNKNOWN";
    }
}

#endif /* MODE_H */
