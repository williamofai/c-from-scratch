/**
 * pulse.c - Heartbeat-Based Liveness Monitor Implementation
 * 
 * This is a direct transcription of the mathematical model.
 * Every line traces to a contract or transition table entry.
 * 
 * See: lessons/02-mathematical-closure/LESSON.md
 * 
 * Copyright (c) 2025 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#include "pulse.h"

/*---------------------------------------------------------------------------
 * Helper Functions
 *---------------------------------------------------------------------------*/

/** Modular age computation: (now - then) mod 2^64 */
static inline uint64_t age_u64(uint64_t now, uint64_t then)
{
    return (uint64_t)(now - then);
}

/** Half-range rule: valid if age < 2^63 */
static inline uint8_t age_valid(uint64_t age)
{
    return (age < (1ULL << 63));
}

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

void hb_init(hb_fsm_t *m, uint64_t now)
{
    m->st = STATE_UNKNOWN;
    m->t_init = now;
    m->last_hb = 0;
    m->have_hb = 0;
    m->fault_time = 0;
    m->fault_reentry = 0;
    m->in_step = 0;
}

void hb_step(hb_fsm_t *m, uint64_t now, uint8_t hb_seen,
             uint64_t T, uint64_t W)
{
    /* Reentrancy check — CONTRACT enforcement */
    if (m->in_step) {
        m->fault_reentry = 1;
        m->st = STATE_DEAD;
        return;
    }
    m->in_step = 1;

    /* Record heartbeat if seen */
    if (hb_seen) {
        m->last_hb = now;
        m->have_hb = 1;
    }

    /* No evidence yet — stay UNKNOWN */
    if (!m->have_hb) {
        uint64_t a_init = age_u64(now, m->t_init);
        if (!age_valid(a_init)) {
            m->fault_time = 1;
            m->st = STATE_DEAD;
        } else {
            m->st = STATE_UNKNOWN;
            /* Note: W (init window) not used here since we have no evidence.
             * We stay UNKNOWN regardless of how long we've waited. */
            (void)W;  /* Suppress unused parameter warning */
        }
        m->in_step = 0;
        return;
    }

    /* Have evidence — check age */
    uint64_t a_hb = age_u64(now, m->last_hb);
    if (!age_valid(a_hb)) {
        m->fault_time = 1;
        m->st = STATE_DEAD;
        m->in_step = 0;
        return;
    }

    /* Transition based on timeout — direct from transition table */
    m->st = (a_hb > T) ? STATE_DEAD : STATE_ALIVE;
    m->in_step = 0;
}
