/**
 * pulse.h - Heartbeat Monitor (Module 1)
 * 
 * Detects existence/liveness of a signal source.
 * 
 * CONTRACT: If no heartbeat received within timeout T, declare DEAD.
 * 
 * Copyright (c) 2026 William Murray
 * MIT License
 */

#ifndef PULSE_H
#define PULSE_H

#include <stdint.h>

typedef enum {
    PULSE_INIT  = 0,
    PULSE_ALIVE = 1,
    PULSE_DEAD  = 2
} pulse_state_t;

typedef struct {
    uint64_t timeout_ms;      /* Timeout threshold */
    uint64_t last_beat;       /* Last heartbeat timestamp */
    pulse_state_t state;      /* Current state */
    uint64_t beats;           /* Total beats received */
    uint8_t initialized;
} pulse_t;

static inline int pulse_init(pulse_t *p, uint64_t timeout_ms) {
    if (!p || timeout_ms == 0) return -1;
    p->timeout_ms = timeout_ms;
    p->last_beat = 0;
    p->state = PULSE_INIT;
    p->beats = 0;
    p->initialized = 1;
    return 0;
}

static inline int pulse_beat(pulse_t *p, uint64_t now_ms) {
    if (!p || !p->initialized) return -1;
    p->last_beat = now_ms;
    p->beats++;
    p->state = PULSE_ALIVE;
    return 0;
}

static inline int pulse_check(pulse_t *p, uint64_t now_ms) {
    if (!p || !p->initialized) return -1;
    if (p->state == PULSE_INIT) return 0;
    
    uint64_t elapsed = now_ms - p->last_beat;
    if (elapsed > p->timeout_ms) {
        p->state = PULSE_DEAD;
    }
    return 0;
}

static inline pulse_state_t pulse_state(const pulse_t *p) {
    return p ? p->state : PULSE_DEAD;
}

static inline const char* pulse_state_name(pulse_state_t s) {
    switch (s) {
        case PULSE_INIT:  return "INIT";
        case PULSE_ALIVE: return "ALIVE";
        case PULSE_DEAD:  return "DEAD";
        default:          return "UNKNOWN";
    }
}

#endif /* PULSE_H */
