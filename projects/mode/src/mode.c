/**
 * mode.c - System Mode Manager Implementation
 * 
 * The "Captain" of the safety-critical ship.
 * 
 * TRANSITION RULES:
 *   INIT → STARTUP:       All modules report OK/LEARNING
 *   STARTUP → OPERATIONAL: All HEALTHY, no flags, min_dwell met
 *   OPERATIONAL → DEGRADED: Any DEGRADED state OR warning flags
 *   DEGRADED → OPERATIONAL: All HEALTHY, no flags, min_dwell met
 *   ANY → EMERGENCY:      Any FAULTY state (immediate)
 *   EMERGENCY → INIT:     Only via explicit reset
 * 
 * FORBIDDEN TRANSITIONS:
 *   INIT → OPERATIONAL    (must pass through STARTUP)
 *   EMERGENCY → *         (without reset)
 *   * → STARTUP           (from OPERATIONAL/DEGRADED)
 * 
 * Copyright (c) 2026 William Murray
 * MIT License
 */

#include "mode.h"
#include <string.h>

/*===========================================================================
 * Internal Helpers
 *===========================================================================*/

/**
 * Check if all modules are in a healthy or learning state.
 */
static int all_modules_ok(const mode_input_t *input) {
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        health_state_t h = input->states[i];
        if (h == HEALTH_FAULTY || h == HEALTH_DEGRADED) {
            return 0;
        }
    }
    return 1;
}

/**
 * Check if all modules are fully healthy (not learning).
 */
static int all_modules_healthy(const mode_input_t *input) {
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        if (input->states[i] != HEALTH_HEALTHY) {
            return 0;
        }
    }
    return 1;
}

/**
 * Check if any module is faulty.
 */
static int any_module_faulty(const mode_input_t *input, uint8_t *trigger_mask) {
    *trigger_mask = 0;
    int faulty = 0;
    
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        if (input->states[i] == HEALTH_FAULTY) {
            *trigger_mask |= (1 << i);
            faulty = 1;
        }
    }
    
    return faulty;
}

/**
 * Check if any module is degraded.
 */
static int any_module_degraded(const mode_input_t *input, uint8_t *trigger_mask) {
    *trigger_mask = 0;
    int degraded = 0;
    
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        if (input->states[i] == HEALTH_DEGRADED) {
            *trigger_mask |= (1 << i);
            degraded = 1;
        }
    }
    
    return degraded;
}

/**
 * Check if any critical flags are set.
 */
static int any_critical_flags(const mode_flags_t *flags) {
    return flags->approaching_upper ||
           flags->approaching_lower ||
           flags->low_confidence ||
           flags->queue_critical ||
           flags->timing_unstable ||
           flags->baseline_volatile;
}

/**
 * Log a transition to history.
 */
static void log_transition(mode_manager_t *m, 
                           system_mode_t from, 
                           system_mode_t to,
                           uint8_t trigger_mask,
                           uint8_t flags_snapshot,
                           uint64_t timestamp) {
    mode_transition_t *entry = &m->history[m->history_head];
    
    entry->timestamp = timestamp;
    entry->from_mode = from;
    entry->to_mode = to;
    entry->trigger_mask = trigger_mask;
    entry->flags_snapshot = flags_snapshot;
    
    m->history_head = (m->history_head + 1) % MODE_HISTORY_SIZE;
    if (m->history_count < MODE_HISTORY_SIZE) {
        m->history_count++;
    }
    
    m->total_transitions++;
    
    if (to == MODE_EMERGENCY) {
        m->emergency_count++;
    }
}

/**
 * Perform a mode transition.
 */
static void transition(mode_manager_t *m, 
                       system_mode_t new_mode,
                       uint8_t trigger_mask,
                       const mode_input_t *input) {
    /* Convert flags to byte for logging */
    uint8_t flags_byte = 0;
    if (input->flags.approaching_upper) flags_byte |= 0x01;
    if (input->flags.approaching_lower) flags_byte |= 0x02;
    if (input->flags.low_confidence)    flags_byte |= 0x04;
    if (input->flags.queue_critical)    flags_byte |= 0x08;
    if (input->flags.timing_unstable)   flags_byte |= 0x10;
    if (input->flags.baseline_volatile) flags_byte |= 0x20;
    
    log_transition(m, m->mode, new_mode, trigger_mask, flags_byte, input->timestamp);
    
    m->mode = new_mode;
    m->ticks_in_mode = 0;
    
    if (new_mode == MODE_EMERGENCY) {
        m->fault_active = 1;
    }
}

/*===========================================================================
 * Public API
 *===========================================================================*/

int mode_init(mode_manager_t *m, const mode_config_t *cfg) {
    if (!m) return MODE_ERR_NULL;
    
    /* Use defaults if no config provided */
    if (cfg) {
        /* Validate config */
        if (cfg->min_dwell_startup < 1 || cfg->min_dwell_degraded < 1) {
            return MODE_ERR_CONFIG;
        }
        m->cfg = *cfg;
    } else {
        m->cfg = MODE_DEFAULT_CONFIG;
    }
    
    /* Initial state */
    m->mode = MODE_INIT;
    m->ticks_in_mode = 0;
    m->fault_active = 0;
    
    /* Clear history */
    memset(m->history, 0, sizeof(m->history));
    m->history_head = 0;
    m->history_count = 0;
    
    /* Statistics */
    m->total_transitions = 0;
    m->emergency_count = 0;
    
    m->initialized = 1;
    
    return MODE_OK;
}

int mode_update(mode_manager_t *m, const mode_input_t *input, mode_result_t *result) {
    if (!m || !input) return MODE_ERR_NULL;
    if (!m->initialized) return MODE_ERR_STATE;
    
    uint8_t trigger_mask = 0;
    int transitioned = 0;
    
    /*-----------------------------------------------------------------------
     * EMERGENCY CHECK (highest priority, from any state)
     *-----------------------------------------------------------------------*/
    if (m->mode != MODE_EMERGENCY && m->mode != MODE_TEST) {
        if (any_module_faulty(input, &trigger_mask)) {
            transition(m, MODE_EMERGENCY, trigger_mask, input);
            transitioned = 1;
            goto done;
        }
    }
    
    /*-----------------------------------------------------------------------
     * State-specific logic
     *-----------------------------------------------------------------------*/
    switch (m->mode) {
        case MODE_INIT:
            /* INIT → STARTUP: When all modules are OK or LEARNING */
            if (all_modules_ok(input)) {
                transition(m, MODE_STARTUP, 0, input);
                transitioned = 1;
            }
            break;
            
        case MODE_STARTUP:
            /* STARTUP → OPERATIONAL: All healthy, no flags, dwell met */
            if (m->ticks_in_mode >= m->cfg.min_dwell_startup) {
                int ready = all_modules_healthy(input);
                
                if (m->cfg.use_value_flags && any_critical_flags(&input->flags)) {
                    ready = 0;
                }
                
                if (ready) {
                    transition(m, MODE_OPERATIONAL, 0, input);
                    transitioned = 1;
                }
            }
            
            /* STARTUP → DEGRADED: If any module degrades during startup */
            if (!transitioned && any_module_degraded(input, &trigger_mask)) {
                transition(m, MODE_DEGRADED, trigger_mask, input);
                transitioned = 1;
            }
            break;
            
        case MODE_OPERATIONAL:
            /* OPERATIONAL → DEGRADED: Any degraded state OR warning flags */
            if (any_module_degraded(input, &trigger_mask)) {
                transition(m, MODE_DEGRADED, trigger_mask, input);
                transitioned = 1;
            } else if (m->cfg.use_value_flags && any_critical_flags(&input->flags)) {
                transition(m, MODE_DEGRADED, TRIGGER_FLAGS, input);
                transitioned = 1;
            }
            break;
            
        case MODE_DEGRADED:
            /* DEGRADED → OPERATIONAL: All healthy, no flags, dwell met */
            if (m->ticks_in_mode >= m->cfg.min_dwell_degraded) {
                int recovered = all_modules_healthy(input);
                
                if (m->cfg.use_value_flags && any_critical_flags(&input->flags)) {
                    recovered = 0;
                }
                
                if (recovered) {
                    transition(m, MODE_OPERATIONAL, 0, input);
                    transitioned = 1;
                }
            }
            break;
            
        case MODE_EMERGENCY:
            /* EMERGENCY is sticky — only exit via mode_reset() */
            /* No automatic transitions */
            break;
            
        case MODE_TEST:
            /* TEST mode ignores normal transitions */
            /* Exit only via mode_exit_test() */
            break;
            
        default:
            return MODE_ERR_STATE;
    }
    
done:
    /* Increment dwell time if no transition */
    if (!transitioned) {
        m->ticks_in_mode++;
    }
    
    /* Fill result */
    if (result) {
        result->mode = m->mode;
        result->permissions = MODE_PERMISSIONS[m->mode];
        result->ticks_in_mode = m->ticks_in_mode;
        result->transitioned = transitioned;
        result->fault_active = m->fault_active;
    }
    
    return MODE_OK;
}

void mode_reset(mode_manager_t *m) {
    if (!m) return;
    
    /* Log the reset transition */
    log_transition(m, m->mode, MODE_INIT, TRIGGER_RESET, 0, 0);
    
    m->mode = MODE_INIT;
    m->ticks_in_mode = 0;
    m->fault_active = 0;
}

int mode_get_history(const mode_manager_t *m, mode_transition_t *out, int max_count) {
    if (!m || !out || max_count <= 0) return 0;
    
    int count = (m->history_count < max_count) ? m->history_count : max_count;
    
    /* Read from oldest to newest */
    int start = (m->history_head - m->history_count + MODE_HISTORY_SIZE) % MODE_HISTORY_SIZE;
    
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % MODE_HISTORY_SIZE;
        out[i] = m->history[idx];
    }
    
    return count;
}

int mode_enter_test(mode_manager_t *m) {
    if (!m) return MODE_ERR_NULL;
    
    /* Cannot enter TEST from EMERGENCY */
    if (m->mode == MODE_EMERGENCY) {
        return MODE_ERR_LOCKED;
    }
    
    log_transition(m, m->mode, MODE_TEST, 0, 0, 0);
    
    m->mode = MODE_TEST;
    m->ticks_in_mode = 0;
    
    return MODE_OK;
}

void mode_exit_test(mode_manager_t *m) {
    if (!m) return;
    if (m->mode != MODE_TEST) return;
    
    log_transition(m, MODE_TEST, MODE_INIT, TRIGGER_RESET, 0, 0);
    
    m->mode = MODE_INIT;
    m->ticks_in_mode = 0;
}
