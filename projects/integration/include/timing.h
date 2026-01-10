/**
 * timing.h - Timing Health Monitor (Module 3)
 * 
 * Monitors timing regularity of periodic events.
 * Flags jitter violations and missed deadlines.
 * 
 * CONTRACT: If |interval - expected| > tolerance, declare UNHEALTHY.
 * 
 * Copyright (c) 2026 William Murray
 * MIT License
 */

#ifndef TIMING_H
#define TIMING_H

#include <stdint.h>

typedef enum {
    TIMING_INIT      = 0,
    TIMING_HEALTHY   = 1,
    TIMING_UNHEALTHY = 2
} timing_state_t;

typedef struct {
    uint64_t expected_interval_ms;  /* Expected period */
    uint64_t tolerance_ms;          /* Allowed jitter */
    uint64_t last_event;            /* Last event timestamp */
    uint64_t events;                /* Event count */
    uint64_t violations;            /* Jitter violations */
    int64_t last_jitter;            /* Most recent jitter */
    timing_state_t state;
    uint8_t initialized;
} timing_t;

static inline int timing_init(timing_t *t, uint64_t expected_interval_ms, uint64_t tolerance_ms) {
    if (!t || expected_interval_ms == 0) return -1;
    t->expected_interval_ms = expected_interval_ms;
    t->tolerance_ms = tolerance_ms;
    t->last_event = 0;
    t->events = 0;
    t->violations = 0;
    t->last_jitter = 0;
    t->state = TIMING_INIT;
    t->initialized = 1;
    return 0;
}

static inline int timing_event(timing_t *t, uint64_t now_ms) {
    if (!t || !t->initialized) return -1;
    
    if (t->events > 0) {
        uint64_t actual_interval = now_ms - t->last_event;
        int64_t jitter = (int64_t)actual_interval - (int64_t)t->expected_interval_ms;
        t->last_jitter = jitter;
        
        uint64_t abs_jitter = (jitter < 0) ? (uint64_t)(-jitter) : (uint64_t)jitter;
        
        if (abs_jitter > t->tolerance_ms) {
            t->violations++;
            t->state = TIMING_UNHEALTHY;
        } else {
            t->state = TIMING_HEALTHY;
        }
    } else {
        t->state = TIMING_HEALTHY;
    }
    
    t->last_event = now_ms;
    t->events++;
    
    return 0;
}

static inline timing_state_t timing_state(const timing_t *t) {
    return t ? t->state : TIMING_UNHEALTHY;
}

static inline const char* timing_state_name(timing_state_t s) {
    switch (s) {
        case TIMING_INIT:      return "INIT";
        case TIMING_HEALTHY:   return "HEALTHY";
        case TIMING_UNHEALTHY: return "UNHEALTHY";
        default:               return "UNKNOWN";
    }
}

#endif /* TIMING_H */
