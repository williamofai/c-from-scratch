/**
 * test_timing.c - Contract and Integration Tests for Timing Monitor
 * 
 * Tests the 6 contracts plus integration scenarios:
 *   CONTRACT-1: Existence Inheritance
 *   CONTRACT-2: Normality Inheritance
 *   CONTRACT-3: Health Requires Evidence
 *   CONTRACT-4: Bounded Detection
 *   CONTRACT-5: Spike Resistance
 *   CONTRACT-6: Deterministic Composition
 * 
 * Copyright (c) 2025 William Murray
 * MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "timing.h"

/* ============================================================
 * TEST INFRASTRUCTURE
 * ============================================================ */

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s - %s\n", __func__, msg); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define PASS(msg) do { \
    printf("[PASS] %s: %s\n", __func__, msg); \
    tests_passed++; \
} while(0)

#define TEST(func) do { \
    tests_run++; \
    func(); \
} while(0)

/* ============================================================
 * CONTRACT-1: Existence Inheritance
 * 
 * "If Pulse reports DEAD, Timing reports DEAD."
 * ============================================================ */

static void test_contract1_existence_inheritance(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.heartbeat_timeout_ms = 2000;
    cfg.n_min = 20;
    
    ASSERT(timing_init(&t, &cfg) == 0, "init failed");
    
    /* Send some heartbeats to establish state */
    uint64_t ts = 0;
    for (int i = 0; i < 25; i++) {
        ts += 1000;
        timing_heartbeat(&t, ts);
    }
    
    ASSERT(t.state == TIMING_HEALTHY, "should be healthy before timeout");
    
    /* Now let it timeout - no heartbeat for > 2000ms */
    ts += 3000;
    timing_result_t r = timing_check(&t, ts);
    
    ASSERT(r.state == TIMING_DEAD, "should be DEAD after timeout");
    ASSERT(r.is_dead == 1, "is_dead flag should be set");
    ASSERT(hb_state(&t.pulse) == STATE_DEAD, "pulse should be DEAD");
    
    PASS("Dead pulse → Dead timing");
}

/* ============================================================
 * CONTRACT-2: Normality Inheritance
 * 
 * "If Pulse is ALIVE and Baseline reports DEVIATION, 
 *  Timing reports UNHEALTHY."
 * ============================================================ */

static void test_contract2_normality_inheritance(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    cfg.k = 2.0;  /* Lower threshold for easier testing */
    
    ASSERT(timing_init(&t, &cfg) == 0, "init failed");
    
    /* Establish baseline at 1000ms */
    uint64_t ts = 0;
    for (int i = 0; i < 25; i++) {
        ts += 1000;
        timing_heartbeat(&t, ts);
    }
    
    ASSERT(t.state == TIMING_HEALTHY, "should be healthy with stable timing");
    
    /* Inject severe deviation */
    ts += 100;  /* Way too fast - should be ~1000ms */
    timing_result_t r = timing_heartbeat(&t, ts);
    
    /* Pulse should still be ALIVE */
    ASSERT(hb_state(&t.pulse) == STATE_ALIVE, "pulse should still be ALIVE");
    
    /* Timing should detect the anomaly */
    ASSERT(r.state == TIMING_UNHEALTHY, "should be UNHEALTHY on timing deviation");
    ASSERT(r.is_unhealthy == 1, "is_unhealthy flag should be set");
    
    PASS("Timing deviation → Unhealthy (while pulse alive)");
}

/* ============================================================
 * CONTRACT-3: Health Requires Evidence
 * 
 * "Timing reports HEALTHY only when:
 *   - Pulse has seen ≥ 1 heartbeat
 *   - Baseline has seen ≥ n_min observations
 *   - Baseline z-score ≤ k"
 * ============================================================ */

static void test_contract3_health_requires_evidence(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    
    ASSERT(timing_init(&t, &cfg) == 0, "init failed");
    
    /* Initially should be INITIALIZING */
    ASSERT(t.state == TIMING_INITIALIZING, "should start INITIALIZING");
    
    /* First heartbeat - still initializing (baseline learning) */
    timing_result_t r = timing_heartbeat(&t, 1000);
    ASSERT(r.state == TIMING_INITIALIZING, "should be INITIALIZING after 1 heartbeat");
    
    /* Not enough observations yet - send only 10 (less than n_min=20) */
    uint64_t ts = 1000;
    for (int i = 0; i < 10; i++) {
        ts += 1000;
        r = timing_heartbeat(&t, ts);
    }
    ASSERT(r.state == TIMING_INITIALIZING, "should be INITIALIZING before n_min");
    
    /* After n_min observations, should become HEALTHY */
    for (int i = 0; i < 15; i++) {
        ts += 1000;
        r = timing_heartbeat(&t, ts);
    }
    ASSERT(r.state == TIMING_HEALTHY, "should be HEALTHY after n_min with stable timing");
    ASSERT(timing_ready(&t) == 1, "timing_ready should return true");
    
    PASS("Health requires pulse evidence + baseline evidence + stable z-score");
}

/* ============================================================
 * CONTRACT-4: Bounded Detection Latency
 * 
 * "A sustained timing anomaly is detected within O(1/α) heartbeats."
 * ============================================================ */

static void test_contract4_bounded_detection(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    cfg.alpha = 0.1;
    cfg.k = 2.0;  /* Lower threshold for faster detection */
    
    ASSERT(timing_init(&t, &cfg) == 0, "init failed");
    
    /* Establish baseline at 1000ms - need >n_min to be HEALTHY */
    uint64_t ts = 0;
    for (int i = 0; i < 25; i++) {
        ts += 1000;
        timing_heartbeat(&t, ts);
    }
    
    ASSERT(t.state == TIMING_HEALTHY, "should be healthy");
    
    /* Switch to 3000ms intervals (sustained anomaly - 3x normal) */
    int steps_to_detect = 0;
    int max_steps = 100;  /* Safety limit */
    
    for (int i = 0; i < max_steps; i++) {
        ts += 3000;  /* Triple the normal interval */
        timing_result_t r = timing_heartbeat(&t, ts);
        steps_to_detect++;
        
        if (r.state == TIMING_UNHEALTHY) {
            break;
        }
    }
    
    /* Should detect within roughly 2/alpha = 20 steps (with margin) */
    int expected_bound = (int)ceil(2.0 / cfg.alpha);  /* ~20 steps theoretical max */
    ASSERT(steps_to_detect <= expected_bound + 10, "detection should be bounded");
    
    char msg[100];
    snprintf(msg, sizeof(msg), "Detected sustained anomaly in %d steps (bound ~%d)", 
             steps_to_detect, expected_bound);
    PASS(msg);
}

/* ============================================================
 * CONTRACT-5: Spike Resistance
 * 
 * "A single anomalous Δt shifts the baseline by at most α·|Δt - μ|."
 * ============================================================ */

static void test_contract5_spike_resistance(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    cfg.alpha = 0.1;
    cfg.k = 5.0;  /* High threshold so spike doesn't immediately trigger deviation */
    
    ASSERT(timing_init(&t, &cfg) == 0, "init failed");
    
    /* Establish baseline at 1000ms */
    uint64_t ts = 0;
    for (int i = 0; i < 20; i++) {
        ts += 1000;
        timing_heartbeat(&t, ts);
    }
    
    double mu_before = t.baseline.mu;
    
    /* Inject single large spike */
    ts += 5000;  /* 5000ms instead of 1000ms */
    timing_heartbeat(&t, ts);
    
    double mu_after = t.baseline.mu;
    double delta_mu = fabs(mu_after - mu_before);
    double spike_size = 5000.0 - mu_before;
    double max_shift = cfg.alpha * fabs(spike_size);
    
    ASSERT(delta_mu <= max_shift * 1.01, "mean shift should be bounded by α·spike");
    
    /* Return to normal - baseline should recover */
    for (int i = 0; i < 25; i++) {
        ts += 1000;
        timing_heartbeat(&t, ts);
    }
    
    /* Should return to HEALTHY */
    ASSERT(t.state == TIMING_HEALTHY, "should recover after spike");
    
    char msg[100];
    snprintf(msg, sizeof(msg), "Spike shift=%.1f <= max=%.1f, recovered to HEALTHY", 
             delta_mu, max_shift);
    PASS(msg);
}

/* ============================================================
 * CONTRACT-6: Deterministic Composition
 * 
 * "Given identical event sequences and initial states,
 *  Timing produces identical output sequences."
 * ============================================================ */

static void test_contract6_deterministic(void) {
    timing_fsm_t t1, t2;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    
    ASSERT(timing_init(&t1, &cfg) == 0, "init t1 failed");
    ASSERT(timing_init(&t2, &cfg) == 0, "init t2 failed");
    
    /* Feed identical sequences to both FSMs */
    uint64_t timestamps[] = {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 
                             9000, 10000, 11000, 12000, 10500, 13000, 14000};
    int n = sizeof(timestamps) / sizeof(timestamps[0]);
    
    for (int i = 0; i < n; i++) {
        timing_result_t r1 = timing_heartbeat(&t1, timestamps[i]);
        timing_result_t r2 = timing_heartbeat(&t2, timestamps[i]);
        
        ASSERT(r1.state == r2.state, "states should match");
        ASSERT(r1.has_dt == r2.has_dt, "has_dt should match");
        if (r1.has_dt) {
            ASSERT(fabs(r1.dt - r2.dt) < 0.001, "dt should match");
        }
        ASSERT(r1.has_z == r2.has_z, "has_z should match");
        if (r1.has_z) {
            ASSERT(fabs(r1.z - r2.z) < 0.001, "z should match");
        }
    }
    
    /* Final states should be identical */
    ASSERT(t1.state == t2.state, "final states should match");
    ASSERT(fabs(t1.baseline.mu - t2.baseline.mu) < 0.001, "baseline mu should match");
    ASSERT(t1.heartbeat_count == t2.heartbeat_count, "heartbeat counts should match");
    
    PASS("Identical inputs → identical outputs");
}

/* ============================================================
 * INTEGRATION TESTS
 * ============================================================ */

static void test_integration_normal_rhythm(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    
    ASSERT(timing_init(&t, &cfg) == 0, "init failed");
    
    /* Regular heartbeats */
    uint64_t ts = 0;
    int healthy_count = 0;
    
    for (int i = 0; i < 40; i++) {
        ts += 1000;
        timing_result_t r = timing_heartbeat(&t, ts);
        if (r.state == TIMING_HEALTHY) {
            healthy_count++;
        }
    }
    
    ASSERT(healthy_count >= 15, "should be healthy for most observations");
    ASSERT(t.state == TIMING_HEALTHY, "should end healthy");
    
    PASS("Normal rhythm stays healthy");
}

static void test_integration_recovery(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    cfg.k = 2.0;  /* More sensitive */
    
    ASSERT(timing_init(&t, &cfg) == 0, "init failed");
    
    /* Establish baseline */
    uint64_t ts = 0;
    for (int i = 0; i < 25; i++) {
        ts += 1000;
        timing_heartbeat(&t, ts);
    }
    
    ASSERT(t.state == TIMING_HEALTHY, "should be healthy");
    
    /* Trigger deviation - very short interval */
    ts += 50;  /* 50ms instead of 1000ms - huge deviation */
    timing_result_t r = timing_heartbeat(&t, ts);
    ASSERT(r.state == TIMING_UNHEALTHY, "should be unhealthy");
    
    /* Return to normal */
    for (int i = 0; i < 25; i++) {
        ts += 1000;
        r = timing_heartbeat(&t, ts);
    }
    
    ASSERT(t.state == TIMING_HEALTHY, "should recover to healthy");
    
    PASS("Recovers from temporary anomaly");
}

static void test_integration_reset(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    
    ASSERT(timing_init(&t, &cfg) == 0, "init failed");
    
    /* Build up state */
    uint64_t ts = 0;
    for (int i = 0; i < 25; i++) {
        ts += 1000;
        timing_heartbeat(&t, ts);
    }
    
    ASSERT(t.state == TIMING_HEALTHY, "should be healthy");
    ASSERT(t.heartbeat_count > 0, "should have heartbeat count");
    
    /* Reset */
    timing_reset(&t);
    
    ASSERT(t.state == TIMING_INITIALIZING, "should be INITIALIZING after reset");
    ASSERT(t.heartbeat_count == 0, "heartbeat count should be 0");
    ASSERT(t.has_prev_heartbeat == 0, "has_prev_heartbeat should be 0");
    ASSERT(t.fault_pulse == 0, "faults should be cleared");
    ASSERT(t.fault_baseline == 0, "faults should be cleared");
    
    PASS("Reset clears state and statistics");
}

/* ============================================================
 * FUZZ TESTS
 * ============================================================ */

static void test_fuzz_random_timestamps(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    
    ASSERT(timing_init(&t, &cfg) == 0, "init failed");
    
    srand(12345);
    uint64_t ts = 0;
    int valid_states = 0;
    
    for (int i = 0; i < 10000; i++) {
        /* Random interval between 100ms and 3000ms */
        uint64_t interval = (uint64_t)(100 + rand() % 2900);
        ts += interval;
        
        timing_result_t r = timing_heartbeat(&t, ts);
        
        /* State should always be valid */
        if (r.state >= TIMING_INITIALIZING && r.state <= TIMING_DEAD) {
            valid_states++;
        }
        
        /* Check invariants */
        ASSERT(t.in_step == 0, "in_step should be 0 after call");
    }
    
    ASSERT(valid_states == 10000, "all states should be valid");
    
    char msg[64];
    snprintf(msg, sizeof(msg), "10000 random timestamps, all states valid");
    PASS(msg);
}

static void test_fuzz_edge_timestamps(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    cfg.n_min = 20;
    
    ASSERT(timing_init(&t, &cfg) == 0, "init failed");
    
    /* Test with edge case timestamps */
    uint64_t edge_cases[] = {0, 1, 1000, 1001, UINT32_MAX, UINT64_MAX / 2};
    int n = sizeof(edge_cases) / sizeof(edge_cases[0]);
    
    uint64_t prev = 0;
    for (int i = 0; i < n; i++) {
        if (edge_cases[i] >= prev) {  /* Timestamps must be non-decreasing */
            timing_result_t r = timing_heartbeat(&t, edge_cases[i]);
            ASSERT(r.state >= TIMING_INITIALIZING && r.state <= TIMING_DEAD, 
                   "state should be valid");
            prev = edge_cases[i];
        }
    }
    
    PASS("Edge case timestamps handled correctly");
}

/* ============================================================
 * CONFIG VALIDATION TESTS
 * ============================================================ */

static void test_config_validation(void) {
    timing_fsm_t t;
    timing_config_t cfg = TIMING_DEFAULT_CONFIG;
    
    /* Valid config should work */
    ASSERT(timing_init(&t, &cfg) == 0, "valid config should succeed");
    
    /* Invalid: zero timeout */
    cfg = TIMING_DEFAULT_CONFIG;
    cfg.heartbeat_timeout_ms = 0;
    ASSERT(timing_init(&t, &cfg) == -1, "zero timeout should fail");
    
    /* Invalid: alpha out of range */
    cfg = TIMING_DEFAULT_CONFIG;
    cfg.alpha = 0.0;
    ASSERT(timing_init(&t, &cfg) == -1, "alpha=0 should fail");
    
    cfg.alpha = 1.0;
    ASSERT(timing_init(&t, &cfg) == -1, "alpha=1 should fail");
    
    cfg.alpha = 1.5;
    ASSERT(timing_init(&t, &cfg) == -1, "alpha>1 should fail");
    
    /* Invalid: zero epsilon */
    cfg = TIMING_DEFAULT_CONFIG;
    cfg.epsilon = 0.0;
    ASSERT(timing_init(&t, &cfg) == -1, "epsilon=0 should fail");
    
    /* Invalid: zero k */
    cfg = TIMING_DEFAULT_CONFIG;
    cfg.k = 0.0;
    ASSERT(timing_init(&t, &cfg) == -1, "k=0 should fail");
    
    /* Invalid: n_min too small */
    cfg = TIMING_DEFAULT_CONFIG;
    cfg.alpha = 0.1;
    cfg.n_min = 15;  /* ceil(2/0.1) = 20, so 15 is too small */
    ASSERT(timing_init(&t, &cfg) == -1, "n_min too small should fail");
    
    PASS("Config validation catches invalid parameters");
}

/* ============================================================
 * MAIN
 * ============================================================ */

int main(void) {
    printf("\n");
    printf("╔═════════════════════════════════════════════════════════════╗\n");
    printf("║           Timing Monitor - Contract Tests                   ║\n");
    printf("╚═════════════════════════════════════════════════════════════╝\n\n");
    
    /* Contract tests */
    printf("--- Contract Tests ---\n");
    TEST(test_contract1_existence_inheritance);
    TEST(test_contract2_normality_inheritance);
    TEST(test_contract3_health_requires_evidence);
    TEST(test_contract4_bounded_detection);
    TEST(test_contract5_spike_resistance);
    TEST(test_contract6_deterministic);
    
    /* Integration tests */
    printf("\n--- Integration Tests ---\n");
    TEST(test_integration_normal_rhythm);
    TEST(test_integration_recovery);
    TEST(test_integration_reset);
    
    /* Fuzz tests */
    printf("\n--- Fuzz Tests ---\n");
    TEST(test_fuzz_random_timestamps);
    TEST(test_fuzz_edge_timestamps);
    
    /* Config tests */
    printf("\n--- Config Validation Tests ---\n");
    TEST(test_config_validation);
    
    /* Summary */
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  Results: %d/%d tests passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(" (%d FAILED)", tests_failed);
    }
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    return tests_failed > 0 ? 1 : 0;
}
