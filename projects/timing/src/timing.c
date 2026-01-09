/**
 * timing.c - Composed Timing Health Monitor Implementation
 * 
 * This file implements the composition of Pulse and Baseline into
 * a unified timing health monitor.
 * 
 * THE COMPOSITION:
 *   event → Pulse → Δt → Baseline → timing_anomaly?
 * 
 * The code is not clever. It's wiring:
 *   - Pulse emits inter-arrival times
 *   - Baseline consumes them
 *   - State mapping decides health
 * 
 * Copyright (c) 2025 William Murray
 * MIT License
 */

#include "timing.h"
#include <math.h>

/* ============================================================
 * INTERNAL: State Mapping
 * ============================================================ */

/**
 * Map component states to composed timing state.
 * 
 * This is the core composition logic - a pure function.
 * 
 * | Pulse State | Baseline State | Timing State    |
 * |-------------|----------------|-----------------|
 * | DEAD        | *              | DEAD            |
 * | UNKNOWN     | *              | INITIALIZING    |
 * | ALIVE       | LEARNING       | INITIALIZING    |
 * | ALIVE       | STABLE         | HEALTHY         |
 * | ALIVE       | DEVIATION      | UNHEALTHY       |
 */
static timing_state_t map_states(state_t pulse_st, base_state_t baseline_st) {
    /* Dead pulse always means dead timing (CONTRACT-1) */
    if (pulse_st == STATE_DEAD) {
        return TIMING_DEAD;
    }
    
    /* Unknown pulse means we're still initializing */
    if (pulse_st == STATE_UNKNOWN) {
        return TIMING_INITIALIZING;
    }
    
    /* Pulse is ALIVE - check baseline state */
    switch (baseline_st) {
        case BASE_LEARNING:
            return TIMING_INITIALIZING;
        case BASE_STABLE:
            return TIMING_HEALTHY;
        case BASE_DEVIATION:
            return TIMING_UNHEALTHY;
        default:
            /* Invalid baseline state - treat as unhealthy (fail-safe) */
            return TIMING_UNHEALTHY;
    }
}

/**
 * Build a result struct from current state.
 */
static timing_result_t build_result(const timing_fsm_t *t,
                                     double dt, uint8_t has_dt,
                                     double z, uint8_t has_z) {
    timing_result_t r;
    
    r.state = t->state;
    r.dt = dt;
    r.has_dt = has_dt;
    r.z = z;
    r.has_z = has_z;
    
    r.pulse_state = hb_state(&t->pulse);
    r.baseline_state = base_state(&t->baseline);
    
    r.is_healthy = (t->state == TIMING_HEALTHY);
    r.is_unhealthy = (t->state == TIMING_UNHEALTHY);
    r.is_dead = (t->state == TIMING_DEAD);
    r.is_anomaly = r.is_unhealthy || r.is_dead;
    
    return r;
}

/* ============================================================
 * PUBLIC API
 * ============================================================ */

int timing_init(timing_fsm_t *t, const timing_config_t *cfg) {
    if (!t || !cfg) {
        return -1;
    }
    
    /* Validate Pulse configuration */
    if (cfg->heartbeat_timeout_ms == 0) {
        return -1;
    }
    
    /* Validate Baseline configuration */
    if (cfg->alpha <= 0.0 || cfg->alpha >= 1.0) {
        return -1;
    }
    if (cfg->epsilon <= 0.0) {
        return -1;
    }
    if (cfg->k <= 0.0) {
        return -1;
    }
    
    /* n_min must be at least ceil(2/alpha) for EMA convergence */
    uint32_t min_n_min = (uint32_t)ceil(2.0 / cfg->alpha);
    if (cfg->n_min < min_n_min) {
        return -1;
    }
    
    /* Store configuration */
    t->cfg = *cfg;
    
    /* Initialize Pulse component */
    hb_init(&t->pulse, 0);  /* Will be re-initialized on first heartbeat */
    
    /* Initialize Baseline component */
    base_config_t base_cfg = {
        .alpha   = cfg->alpha,
        .epsilon = cfg->epsilon,
        .k       = cfg->k,
        .n_min   = cfg->n_min
    };
    if (base_init(&t->baseline, &base_cfg) != 0) {
        return -1;
    }
    
    /* Initialize composed state */
    t->state = TIMING_INITIALIZING;
    t->last_heartbeat_ms = 0;
    t->has_prev_heartbeat = 0;
    
    /* Clear fault flags */
    t->fault_pulse = 0;
    t->fault_baseline = 0;
    
    /* Clear atomicity guard */
    t->in_step = 0;
    
    /* Clear statistics */
    t->heartbeat_count = 0;
    t->healthy_count = 0;
    t->unhealthy_count = 0;
    
    return 0;
}

timing_result_t timing_heartbeat(timing_fsm_t *t, uint64_t timestamp_ms) {
    timing_result_t result = {0};
    double dt = 0.0;
    uint8_t has_dt = 0;
    double z = 0.0;
    uint8_t has_z = 0;
    
    if (!t) {
        result.state = TIMING_DEAD;
        result.is_dead = 1;
        result.is_anomaly = 1;
        return result;
    }
    
    /* Reentrancy guard */
    if (t->in_step) {
        t->fault_pulse = 1;
        t->state = TIMING_DEAD;
        return build_result(t, 0, 0, 0, 0);
    }
    t->in_step = 1;
    
    /* Step 1: Compute inter-arrival time (Δt) */
    if (t->has_prev_heartbeat) {
        /* Handle wrap-around safely with unsigned arithmetic */
        uint64_t elapsed = timestamp_ms - t->last_heartbeat_ms;
        dt = (double)elapsed;
        has_dt = 1;
    }
    
    /* Step 2: Feed heartbeat to Pulse component */
    hb_step(&t->pulse, timestamp_ms, 1,
            t->cfg.heartbeat_timeout_ms, t->cfg.init_window_ms);
    
    /* Check for Pulse fault */
    if (hb_faulted(&t->pulse)) {
        t->fault_pulse = 1;
    }
    
    /* Step 3: If we have Δt, feed to Baseline component */
    if (has_dt && !t->fault_pulse) {
        base_result_t base_r = base_step(&t->baseline, dt);
        z = base_r.z;
        has_z = 1;
        
        /* Check for Baseline fault */
        if (base_faulted(&t->baseline)) {
            t->fault_baseline = 1;
        }
    }
    
    /* Step 4: Map component states to timing state */
    state_t pulse_st = hb_state(&t->pulse);
    base_state_t baseline_st = base_state(&t->baseline);
    
    timing_state_t new_state = map_states(pulse_st, baseline_st);
    
    /* Handle faults: force to safe state */
    if (t->fault_pulse) {
        new_state = TIMING_DEAD;
    } else if (t->fault_baseline && new_state == TIMING_HEALTHY) {
        new_state = TIMING_UNHEALTHY;
    }
    
    /* Update state and statistics */
    t->state = new_state;
    
    /* Update heartbeat tracking */
    t->last_heartbeat_ms = timestamp_ms;
    t->has_prev_heartbeat = 1;
    t->heartbeat_count++;
    
    /* Update consecutive counters */
    if (new_state == TIMING_HEALTHY) {
        t->healthy_count++;
        t->unhealthy_count = 0;
    } else if (new_state == TIMING_UNHEALTHY) {
        t->unhealthy_count++;
        t->healthy_count = 0;
    } else {
        t->healthy_count = 0;
        t->unhealthy_count = 0;
    }
    
    t->in_step = 0;
    
    /* Step 5: Build and return result */
    return build_result(t, dt, has_dt, z, has_z);
}

timing_result_t timing_check(timing_fsm_t *t, uint64_t current_time_ms) {
    if (!t) {
        timing_result_t result = {0};
        result.state = TIMING_DEAD;
        result.is_dead = 1;
        result.is_anomaly = 1;
        return result;
    }
    
    /* Reentrancy guard */
    if (t->in_step) {
        t->fault_pulse = 1;
        t->state = TIMING_DEAD;
        return build_result(t, 0, 0, 0, 0);
    }
    t->in_step = 1;
    
    /* Check pulse timeout (no heartbeat seen) */
    hb_step(&t->pulse, current_time_ms, 0,
            t->cfg.heartbeat_timeout_ms, t->cfg.init_window_ms);
    
    /* Check for Pulse fault */
    if (hb_faulted(&t->pulse)) {
        t->fault_pulse = 1;
    }
    
    /* Map states (baseline state unchanged since no new observation) */
    state_t pulse_st = hb_state(&t->pulse);
    base_state_t baseline_st = base_state(&t->baseline);
    
    timing_state_t new_state = map_states(pulse_st, baseline_st);
    
    /* Handle faults */
    if (t->fault_pulse) {
        new_state = TIMING_DEAD;
    }
    
    /* Update state */
    t->state = new_state;
    
    /* Update consecutive counters */
    if (new_state == TIMING_HEALTHY) {
        t->healthy_count++;
        t->unhealthy_count = 0;
    } else if (new_state == TIMING_UNHEALTHY) {
        t->unhealthy_count++;
        t->healthy_count = 0;
    } else if (new_state == TIMING_DEAD) {
        t->healthy_count = 0;
        t->unhealthy_count = 0;
    }
    
    t->in_step = 0;
    
    return build_result(t, 0, 0, 0, 0);
}

void timing_reset(timing_fsm_t *t) {
    if (!t) return;
    
    /* Re-initialize Pulse */
    hb_init(&t->pulse, 0);
    
    /* Reset Baseline */
    base_reset(&t->baseline);
    
    /* Reset composed state */
    t->state = TIMING_INITIALIZING;
    t->last_heartbeat_ms = 0;
    t->has_prev_heartbeat = 0;
    
    /* Clear faults */
    t->fault_pulse = 0;
    t->fault_baseline = 0;
    
    /* Clear statistics */
    t->heartbeat_count = 0;
    t->healthy_count = 0;
    t->unhealthy_count = 0;
}
