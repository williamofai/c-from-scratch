/**
 * main.c - Timing Monitor Demo Program
 * 
 * Demonstrates the composed timing health monitor with four scenarios:
 *   1. Normal rhythm - regular heartbeats stay HEALTHY
 *   2. Jitter anomaly - erratic timing triggers UNHEALTHY
 *   3. Step change - sudden timing change detected, then adapts
 *   4. Death and recovery - timeout triggers DEAD, then recovery
 * 
 * Copyright (c) 2025 William Murray
 * MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include "timing.h"

/* ============================================================
 * DEMO HELPERS
 * ============================================================ */

static void print_header(const char *title) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  %s\n", title);
    printf("═══════════════════════════════════════════════════════════\n\n");
}

static void print_result(uint64_t ts, const timing_result_t *r) {
    printf("t=%8lu  ", (unsigned long)ts);
    
    if (r->has_dt) {
        printf("Δt=%7.1f  ", r->dt);
    } else {
        printf("Δt=   ---  ");
    }
    
    if (r->has_z) {
        printf("z=%6.2f  ", r->z);
    } else {
        printf("z=  ---  ");
    }
    
    printf("state=%-12s", timing_state_name(r->state));
    
    if (r->is_anomaly) {
        printf("  ⚠️");
    }
    
    printf("\n");
}

static void print_stats(const timing_fsm_t *t) {
    printf("\n");
    printf("Statistics:\n");
    printf("  Heartbeats: %u\n", timing_heartbeat_count(t));
    printf("  Consecutive healthy: %u\n", t->healthy_count);
    printf("  Consecutive unhealthy: %u\n", t->unhealthy_count);
    printf("  Baseline ready: %s\n", timing_ready(t) ? "yes" : "no");
    printf("  Faulted: %s\n", timing_faulted(t) ? "yes" : "no");
}

/* ============================================================
 * DEMO 1: Normal Rhythm
 * ============================================================ */

static void demo_normal_rhythm(void) {
    print_header("DEMO 1: Normal Rhythm");
    
    printf("Scenario: Regular heartbeats at ~1000ms intervals (±25ms jitter)\n");
    printf("Expected: INITIALIZING → HEALTHY, stays HEALTHY\n\n");
    
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;  /* Must be >= ceil(2/alpha) = 20 for alpha=0.1 */
    
    if (timing_init(&t, &cfg) != 0) {
        printf("ERROR: Failed to initialize timing FSM\n");
        return;
    }
    
    /* Seed random for reproducible jitter */
    srand(42);
    
    uint64_t ts = 0;
    for (int i = 0; i < 25; i++) {
        /* Add small jitter: ±25ms */
        int jitter = (rand() % 51) - 25;
        ts += (uint64_t)(1000 + jitter);
        
        timing_result_t r = timing_heartbeat(&t, ts);
        print_result(ts, &r);
    }
    
    print_stats(&t);
}

/* ============================================================
 * DEMO 2: Jitter Anomaly
 * ============================================================ */

static void demo_jitter_anomaly(void) {
    print_header("DEMO 2: Jitter Anomaly");
    
    printf("Scenario: Establish baseline, then introduce severe jitter\n");
    printf("Expected: HEALTHY → UNHEALTHY when jitter exceeds threshold\n\n");
    
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    cfg.k = 2.0;  /* More sensitive for demo */
    
    if (timing_init(&t, &cfg) != 0) {
        printf("ERROR: Failed to initialize timing FSM\n");
        return;
    }
    
    uint64_t ts = 0;
    
    /* Phase 1: Establish normal baseline - need >n_min heartbeats */
    printf("--- Phase 1: Establishing baseline (25 heartbeats) ---\n");
    for (int i = 0; i < 25; i++) {
        ts += 1000;
        timing_result_t r = timing_heartbeat(&t, ts);
        print_result(ts, &r);
    }
    
    /* Phase 2: Inject severe jitter - now we're HEALTHY, deviation should trigger UNHEALTHY */
    printf("\n--- Phase 2: Injecting severe jitter ---\n");
    
    ts += 100;   /* Way too fast: 100ms instead of 1000ms */
    timing_result_t r = timing_heartbeat(&t, ts);
    print_result(ts, &r);
    
    ts += 2500;  /* Way too slow: 2500ms instead of 1000ms */
    r = timing_heartbeat(&t, ts);
    print_result(ts, &r);
    
    ts += 150;   /* Very fast again */
    r = timing_heartbeat(&t, ts);
    print_result(ts, &r);
    
    /* Phase 3: Return to normal */
    printf("\n--- Phase 3: Returning to normal ---\n");
    for (int i = 0; i < 15; i++) {
        ts += 1000;
        r = timing_heartbeat(&t, ts);
        print_result(ts, &r);
    }
    
    print_stats(&t);
}

/* ============================================================
 * DEMO 3: Step Change Detection
 * ============================================================ */

static void demo_step_change(void) {
    print_header("DEMO 3: Step Change Detection");
    
    printf("Scenario: Sudden change in timing (1000ms → 200ms)\n");
    printf("Expected: HEALTHY → UNHEALTHY on step change, then adapts back to HEALTHY\n\n");
    
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    cfg.k = 2.0;  /* Sensitive threshold */
    
    if (timing_init(&t, &cfg) != 0) {
        printf("ERROR: Failed to initialize timing FSM\n");
        return;
    }
    
    uint64_t ts = 0;
    
    /* Phase 1: Establish baseline at 1000ms */
    printf("--- Phase 1: Establishing baseline at 1000ms ---\n");
    for (int i = 0; i < 25; i++) {
        ts += 1000;
        timing_result_t r = timing_heartbeat(&t, ts);
        print_result(ts, &r);
    }
    
    /* Phase 2: Sudden step change to 200ms (5x faster) */
    printf("\n--- Phase 2: Step change to 200ms (should trigger UNHEALTHY) ---\n");
    for (int i = 0; i < 10; i++) {
        ts += 200;
        timing_result_t r = timing_heartbeat(&t, ts);
        print_result(ts, &r);
    }
    
    /* Phase 3: Continue at 200ms - baseline adapts */
    printf("\n--- Phase 3: Baseline adapts to new rhythm ---\n");
    for (int i = 0; i < 20; i++) {
        ts += 200;
        timing_result_t r = timing_heartbeat(&t, ts);
        print_result(ts, &r);
    }
    
    print_stats(&t);
}

/* ============================================================
 * DEMO 4: Death and Recovery
 * ============================================================ */

static void demo_death_and_recovery(void) {
    print_header("DEMO 4: Death and Recovery");
    
    printf("Scenario: Heartbeats stop (timeout), then resume\n");
    printf("Expected: HEALTHY → DEAD → INITIALIZING → HEALTHY\n\n");
    
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.heartbeat_timeout_ms = 3000;  /* 3 second timeout */
    cfg.n_min = 20;
    
    if (timing_init(&t, &cfg) != 0) {
        printf("ERROR: Failed to initialize timing FSM\n");
        return;
    }
    
    uint64_t ts = 0;
    
    /* Phase 1: Establish healthy state - need >n_min heartbeats */
    printf("--- Phase 1: Establishing healthy state (25 heartbeats) ---\n");
    for (int i = 0; i < 25; i++) {
        ts += 1000;
        timing_result_t r = timing_heartbeat(&t, ts);
        print_result(ts, &r);
    }
    
    /* Phase 2: Heartbeats stop */
    printf("\n--- Phase 2: Heartbeats stop (checking timeout) ---\n");
    
    for (int i = 0; i < 5; i++) {
        ts += 1000;  /* Check every second */
        timing_result_t r = timing_check(&t, ts);
        printf("t=%8lu  [check]     ", (unsigned long)ts);
        printf("           ");
        printf("state=%-12s", timing_state_name(r.state));
        if (r.is_dead) {
            printf("  💀 TIMEOUT!");
        }
        printf("\n");
    }
    
    /* Phase 3: Heartbeat resumes */
    printf("\n--- Phase 3: Heartbeat resumes (reset and re-learn) ---\n");
    timing_reset(&t);  /* Reset to re-learn baseline */
    
    for (int i = 0; i < 25; i++) {
        ts += 1000;
        timing_result_t r = timing_heartbeat(&t, ts);
        print_result(ts, &r);
    }
    
    print_stats(&t);
}

/* ============================================================
 * MAIN
 * ============================================================ */

int main(void) {
    printf("\n");
    printf("╔═════════════════════════════════════════════════════════════╗\n");
    printf("║       TIMING - Composed Timing Health Monitor Demo          ║\n");
    printf("║                                                             ║\n");
    printf("║   Module 3 of c-from-scratch                                ║\n");
    printf("║   Composition: event → Pulse → Δt → Baseline → anomaly?     ║\n");
    printf("╚═════════════════════════════════════════════════════════════╝\n");
    
    demo_normal_rhythm();
    demo_jitter_anomaly();
    demo_step_change();
    demo_death_and_recovery();
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Demo complete.\n");
    printf("═══════════════════════════════════════════════════════════\n\n");
    
    return 0;
}
