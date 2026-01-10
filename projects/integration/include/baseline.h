/**
 * baseline.h - Baseline Deviation Monitor (Module 2)
 * 
 * Detects whether a value is within normal operating range.
 * Uses EMA to track baseline and flags deviations.
 * 
 * CONTRACT: If |value - baseline| > threshold, declare DEVIATION.
 * 
 * Copyright (c) 2026 William Murray
 * MIT License
 */

#ifndef BASELINE_H
#define BASELINE_H

#include <stdint.h>
#include <math.h>

typedef enum {
    BASELINE_INIT      = 0,
    BASELINE_LEARNING  = 1,
    BASELINE_STABLE    = 2,
    BASELINE_DEVIATION = 3
} baseline_state_t;

typedef struct {
    double alpha;           /* EMA smoothing factor */
    double threshold;       /* Deviation threshold */
    uint32_t learning_n;    /* Samples needed for learning */
    double baseline;        /* Current baseline estimate */
    double last_value;      /* Most recent value */
    double deviation;       /* |value - baseline| */
    uint32_t n;             /* Sample count */
    baseline_state_t state;
    uint8_t initialized;
} baseline_t;

static inline int baseline_init(baseline_t *b, double alpha, double threshold, uint32_t learning_n) {
    if (!b || alpha <= 0.0 || alpha > 1.0 || threshold <= 0.0 || learning_n == 0) return -1;
    b->alpha = alpha;
    b->threshold = threshold;
    b->learning_n = learning_n;
    b->baseline = 0.0;
    b->last_value = 0.0;
    b->deviation = 0.0;
    b->n = 0;
    b->state = BASELINE_INIT;
    b->initialized = 1;
    return 0;
}

static inline int baseline_update(baseline_t *b, double value) {
    if (!b || !b->initialized) return -1;
    if (!isfinite(value)) return -2;
    
    b->last_value = value;
    b->n++;
    
    if (b->n == 1) {
        b->baseline = value;
        b->state = BASELINE_LEARNING;
    } else {
        b->baseline = b->alpha * value + (1.0 - b->alpha) * b->baseline;
    }
    
    b->deviation = fabs(value - b->baseline);
    
    if (b->n < b->learning_n) {
        b->state = BASELINE_LEARNING;
    } else if (b->deviation > b->threshold) {
        b->state = BASELINE_DEVIATION;
    } else {
        b->state = BASELINE_STABLE;
    }
    
    return 0;
}

static inline baseline_state_t baseline_state(const baseline_t *b) {
    return b ? b->state : BASELINE_DEVIATION;
}

static inline double baseline_get_baseline(const baseline_t *b) {
    return b ? b->baseline : 0.0;
}

static inline double baseline_get_deviation(const baseline_t *b) {
    return b ? b->deviation : 0.0;
}

static inline const char* baseline_state_name(baseline_state_t s) {
    switch (s) {
        case BASELINE_INIT:      return "INIT";
        case BASELINE_LEARNING:  return "LEARNING";
        case BASELINE_STABLE:    return "STABLE";
        case BASELINE_DEVIATION: return "DEVIATION";
        default:                 return "UNKNOWN";
    }
}

#endif /* BASELINE_H */
