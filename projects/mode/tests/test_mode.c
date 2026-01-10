/**
 * test_mode.c - Mode Manager Contract Tests
 * 
 * Tests all 8 contracts and invariants.
 * 
 * Copyright (c) 2026 William Murray
 * MIT License
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "mode.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN(name) do { \
    printf("  "); \
    test_##name(); \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s\n", msg); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define PASS(msg) do { \
    printf("[PASS] %s\n", msg); \
    tests_passed++; \
} while(0)

/*===========================================================================
 * Contract Tests
 *===========================================================================*/

TEST(contract_1_unambiguous_state) {
    /* CONTRACT-1: System exists in exactly one mode at any time */
    mode_manager_t m;
    mode_init(&m, NULL);
    
    mode_input_t input = {0};
    mode_result_t r;
    
    /* Run through many updates */
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < MODE_MODULE_COUNT; j++) {
            input.states[j] = (health_state_t)(rand() % 5);
        }
        input.timestamp = i * 100;
        mode_update(&m, &input, &r);
        
        /* Mode must be valid */
        ASSERT(r.mode >= MODE_INIT && r.mode < MODE_COUNT,
               "CONTRACT-1: Mode out of range");
    }
    
    PASS("CONTRACT-1: Unambiguous state (mode always valid)");
}

TEST(contract_2_safe_entry) {
    /* CONTRACT-2: OPERATIONAL requires all monitors HEALTHY */
    mode_manager_t m;
    mode_config_t cfg = MODE_DEFAULT_CONFIG;
    cfg.min_dwell_startup = 1;
    mode_init(&m, &cfg);
    
    mode_input_t input = {0};
    mode_result_t r;
    
    /* Set all but one to healthy */
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        input.states[i] = HEALTH_HEALTHY;
    }
    input.states[MOD_DRIFT] = HEALTH_LEARNING;  /* One still learning */
    
    /* Update until we're past startup dwell */
    for (int i = 0; i < 10; i++) {
        input.timestamp = i * 100;
        mode_update(&m, &input, &r);
    }
    
    /* Should NOT be in OPERATIONAL because Drift is still LEARNING */
    ASSERT(r.mode != MODE_OPERATIONAL,
           "CONTRACT-2: Entered OPERATIONAL with LEARNING module");
    
    /* Now set all healthy */
    input.states[MOD_DRIFT] = HEALTH_HEALTHY;
    input.timestamp = 1000;
    mode_update(&m, &input, &r);
    input.timestamp = 1100;
    mode_update(&m, &input, &r);
    
    ASSERT(r.mode == MODE_OPERATIONAL,
           "CONTRACT-2: Should be OPERATIONAL when all healthy");
    
    PASS("CONTRACT-2: Safe entry (OPERATIONAL requires all healthy)");
}

TEST(contract_3_fault_stickiness) {
    /* CONTRACT-3: EMERGENCY requires explicit reset to exit */
    mode_manager_t m;
    mode_config_t cfg = MODE_DEFAULT_CONFIG;
    cfg.min_dwell_startup = 1;
    mode_init(&m, &cfg);
    
    mode_input_t input = {0};
    mode_result_t r;
    
    /* Get to OPERATIONAL */
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        input.states[i] = HEALTH_HEALTHY;
    }
    for (int i = 0; i < 5; i++) {
        input.timestamp = i * 100;
        mode_update(&m, &input, &r);
    }
    
    /* Trigger fault */
    input.states[MOD_PULSE] = HEALTH_FAULTY;
    input.timestamp = 500;
    mode_update(&m, &input, &r);
    ASSERT(r.mode == MODE_EMERGENCY, "Should be in EMERGENCY");
    
    /* Try to recover without reset */
    input.states[MOD_PULSE] = HEALTH_HEALTHY;
    for (int i = 0; i < 20; i++) {
        input.timestamp = 600 + i * 100;
        mode_update(&m, &input, &r);
        ASSERT(r.mode == MODE_EMERGENCY,
               "CONTRACT-3: EMERGENCY should be sticky");
    }
    
    /* Reset should work */
    mode_reset(&m);
    input.timestamp = 3000;
    mode_update(&m, &input, &r);
    ASSERT(r.mode != MODE_EMERGENCY,
           "CONTRACT-3: Should exit EMERGENCY after reset");
    
    PASS("CONTRACT-3: Fault stickiness (EMERGENCY requires reset)");
}

TEST(contract_4_no_skip) {
    /* CONTRACT-4: Transitions must follow valid paths */
    mode_manager_t m;
    mode_config_t cfg = MODE_DEFAULT_CONFIG;
    cfg.min_dwell_startup = 1;
    mode_init(&m, &cfg);
    
    mode_input_t input = {0};
    mode_result_t r;
    
    /* Start in INIT */
    ASSERT(mode_get(&m) == MODE_INIT, "Should start in INIT");
    
    /* Set all healthy immediately */
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        input.states[i] = HEALTH_HEALTHY;
    }
    input.timestamp = 100;
    mode_update(&m, &input, &r);
    
    /* Should go to STARTUP first, not OPERATIONAL */
    ASSERT(r.mode == MODE_STARTUP,
           "CONTRACT-4: Cannot skip STARTUP");
    
    /* Now can go to OPERATIONAL after dwell */
    input.timestamp = 200;
    mode_update(&m, &input, &r);
    input.timestamp = 300;
    mode_update(&m, &input, &r);
    ASSERT(r.mode == MODE_OPERATIONAL,
           "Should reach OPERATIONAL after dwell");
    
    PASS("CONTRACT-4: No skip (must pass through STARTUP)");
}

TEST(contract_5_bounded_latency) {
    /* CONTRACT-5: Fault → EMERGENCY in ≤ 1 cycle */
    mode_manager_t m;
    mode_config_t cfg = MODE_DEFAULT_CONFIG;
    cfg.min_dwell_startup = 1;
    mode_init(&m, &cfg);
    
    mode_input_t input = {0};
    mode_result_t r;
    
    /* Get to OPERATIONAL */
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        input.states[i] = HEALTH_HEALTHY;
    }
    for (int i = 0; i < 5; i++) {
        input.timestamp = i * 100;
        mode_update(&m, &input, &r);
    }
    ASSERT(r.mode == MODE_OPERATIONAL, "Should be OPERATIONAL");
    
    /* Inject fault */
    input.states[MOD_CONSENSUS] = HEALTH_FAULTY;
    input.timestamp = 500;
    mode_update(&m, &input, &r);
    
    /* Should be in EMERGENCY immediately */
    ASSERT(r.mode == MODE_EMERGENCY,
           "CONTRACT-5: Should reach EMERGENCY in 1 cycle");
    ASSERT(r.transitioned == 1,
           "CONTRACT-5: Should have transitioned");
    
    PASS("CONTRACT-5: Bounded latency (fault → EMERGENCY in 1 cycle)");
}

TEST(contract_6_deterministic) {
    /* CONTRACT-6: Same inputs → Same mode */
    mode_manager_t m1, m2;
    mode_init(&m1, NULL);
    mode_init(&m2, NULL);
    
    mode_input_t input = {0};
    mode_result_t r1, r2;
    
    /* Apply same sequence to both */
    for (int i = 0; i < 50; i++) {
        /* Deterministic "random" input based on i */
        for (int j = 0; j < MODE_MODULE_COUNT; j++) {
            input.states[j] = (health_state_t)((i + j) % 3 + 1);
        }
        input.flags.approaching_upper = (i % 7 == 0);
        input.timestamp = i * 100;
        
        mode_update(&m1, &input, &r1);
        mode_update(&m2, &input, &r2);
        
        ASSERT(r1.mode == r2.mode,
               "CONTRACT-6: Same inputs should give same mode");
    }
    
    PASS("CONTRACT-6: Deterministic (same inputs → same mode)");
}

TEST(contract_7_proactive_safety) {
    /* CONTRACT-7: Value flags trigger DEGRADED before faults */
    mode_manager_t m;
    mode_config_t cfg = MODE_DEFAULT_CONFIG;
    cfg.min_dwell_startup = 1;
    cfg.use_value_flags = 1;
    mode_init(&m, &cfg);
    
    mode_input_t input = {0};
    mode_result_t r;
    
    /* Get to OPERATIONAL */
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        input.states[i] = HEALTH_HEALTHY;
    }
    for (int i = 0; i < 5; i++) {
        input.timestamp = i * 100;
        mode_update(&m, &input, &r);
    }
    ASSERT(r.mode == MODE_OPERATIONAL, "Should be OPERATIONAL");
    
    /* Set approaching_upper flag (but all states still HEALTHY) */
    input.flags.approaching_upper = 1;
    input.timestamp = 500;
    mode_update(&m, &input, &r);
    
    /* Should degrade proactively */
    ASSERT(r.mode == MODE_DEGRADED,
           "CONTRACT-7: Should degrade on approaching_upper flag");
    
    PASS("CONTRACT-7: Proactive safety (flags trigger DEGRADED)");
}

TEST(contract_8_auditability) {
    /* CONTRACT-8: All transitions logged with cause */
    mode_manager_t m;
    mode_config_t cfg = MODE_DEFAULT_CONFIG;
    cfg.min_dwell_startup = 1;
    mode_init(&m, &cfg);
    
    mode_input_t input = {0};
    mode_result_t r;
    
    /* Generate transitions */
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        input.states[i] = HEALTH_LEARNING;
    }
    input.timestamp = 100;
    mode_update(&m, &input, &r);  /* INIT → STARTUP */
    
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        input.states[i] = HEALTH_HEALTHY;
    }
    input.timestamp = 200;
    mode_update(&m, &input, &r);
    input.timestamp = 300;
    mode_update(&m, &input, &r);  /* STARTUP → OPERATIONAL */
    
    /* Check history */
    mode_transition_t history[MODE_HISTORY_SIZE];
    int count = mode_get_history(&m, history, MODE_HISTORY_SIZE);
    
    ASSERT(count >= 2, "Should have at least 2 transitions logged");
    ASSERT(history[0].from_mode == MODE_INIT, "First transition from INIT");
    ASSERT(history[0].to_mode == MODE_STARTUP, "First transition to STARTUP");
    
    PASS("CONTRACT-8: Auditability (all transitions logged)");
}

/*===========================================================================
 * Invariant Tests
 *===========================================================================*/

TEST(invariant_1_mode_valid) {
    /* INV-1: mode ∈ { INIT, STARTUP, OPERATIONAL, DEGRADED, EMERGENCY, TEST } */
    mode_manager_t m;
    mode_init(&m, NULL);
    
    mode_input_t input = {0};
    mode_result_t r;
    
    srand((unsigned)time(NULL));
    
    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < MODE_MODULE_COUNT; j++) {
            input.states[j] = (health_state_t)(rand() % 5);
        }
        input.flags.approaching_upper = rand() % 2;
        input.flags.low_confidence = rand() % 2;
        input.timestamp = i;
        
        mode_update(&m, &input, &r);
        
        ASSERT(r.mode >= 0 && r.mode < MODE_COUNT,
               "INV-1: Mode out of valid range");
    }
    
    PASS("INV-1: Mode always valid (1000 random updates)");
}

TEST(invariant_2_operational_healthy) {
    /* INV-2: (mode == OPERATIONAL) → all states HEALTHY AND no critical flags */
    mode_manager_t m;
    mode_config_t cfg = MODE_DEFAULT_CONFIG;
    cfg.min_dwell_startup = 1;
    cfg.use_value_flags = 1;
    mode_init(&m, &cfg);
    
    mode_input_t input = {0};
    mode_result_t r;
    
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < MODE_MODULE_COUNT; j++) {
            input.states[j] = (health_state_t)((i + j) % 4 + 1);
        }
        input.flags.approaching_upper = (i % 5 == 0);
        input.timestamp = i * 100;
        
        mode_update(&m, &input, &r);
        
        if (r.mode == MODE_OPERATIONAL) {
            /* Verify all healthy */
            for (int j = 0; j < MODE_MODULE_COUNT; j++) {
                ASSERT(input.states[j] == HEALTH_HEALTHY,
                       "INV-2: OPERATIONAL with non-healthy state");
            }
            /* Verify no critical flags */
            ASSERT(!input.flags.approaching_upper,
                   "INV-2: OPERATIONAL with warning flag");
        }
    }
    
    PASS("INV-2: OPERATIONAL implies all healthy, no flags");
}

TEST(invariant_3_emergency_fault) {
    /* INV-3: (mode == EMERGENCY) → (fault_active == true) */
    mode_manager_t m;
    mode_config_t cfg = MODE_DEFAULT_CONFIG;
    cfg.min_dwell_startup = 1;
    mode_init(&m, &cfg);
    
    mode_input_t input = {0};
    mode_result_t r;
    
    /* Get to OPERATIONAL */
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        input.states[i] = HEALTH_HEALTHY;
    }
    for (int i = 0; i < 5; i++) {
        input.timestamp = i * 100;
        mode_update(&m, &input, &r);
    }
    
    /* Trigger fault */
    input.states[0] = HEALTH_FAULTY;
    input.timestamp = 500;
    mode_update(&m, &input, &r);
    
    ASSERT(r.mode == MODE_EMERGENCY, "Should be EMERGENCY");
    ASSERT(r.fault_active == 1, "INV-3: fault_active must be true in EMERGENCY");
    
    PASS("INV-3: EMERGENCY implies fault_active");
}

TEST(invariant_4_dwell_monotonic) {
    /* INV-4: ticks_in_mode increments monotonically until transition */
    mode_manager_t m;
    mode_config_t cfg = MODE_DEFAULT_CONFIG;
    cfg.min_dwell_startup = 100;  /* Stay in STARTUP for a while */
    mode_init(&m, &cfg);
    
    mode_input_t input = {0};
    mode_result_t r;
    
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        input.states[i] = HEALTH_LEARNING;
    }
    
    /* First update → STARTUP, dwell = 0 */
    input.timestamp = 100;
    mode_update(&m, &input, &r);
    
    uint32_t prev_dwell = r.ticks_in_mode;
    
    for (int i = 0; i < 50; i++) {
        input.timestamp = (i + 2) * 100;
        mode_update(&m, &input, &r);
        
        if (!r.transitioned) {
            ASSERT(r.ticks_in_mode == prev_dwell + 1,
                   "INV-4: Dwell should increment by 1");
        }
        prev_dwell = r.ticks_in_mode;
    }
    
    PASS("INV-4: Dwell time increments monotonically");
}

/*===========================================================================
 * Edge Case Tests
 *===========================================================================*/

TEST(edge_null_pointers) {
    mode_manager_t m;
    mode_result_t r;
    mode_input_t input = {0};
    
    ASSERT(mode_init(NULL, NULL) == MODE_ERR_NULL,
           "Should reject NULL manager");
    
    mode_init(&m, NULL);
    
    ASSERT(mode_update(NULL, &input, &r) == MODE_ERR_NULL,
           "Should reject NULL manager in update");
    ASSERT(mode_update(&m, NULL, &r) == MODE_ERR_NULL,
           "Should reject NULL input");
    
    PASS("Edge: NULL pointer handling");
}

TEST(edge_config_validation) {
    mode_manager_t m;
    mode_config_t bad_cfg = {
        .min_dwell_startup = 0,  /* Invalid */
        .min_dwell_degraded = 1
    };
    
    ASSERT(mode_init(&m, &bad_cfg) == MODE_ERR_CONFIG,
           "Should reject invalid config");
    
    bad_cfg.min_dwell_startup = 1;
    bad_cfg.min_dwell_degraded = 0;  /* Invalid */
    
    ASSERT(mode_init(&m, &bad_cfg) == MODE_ERR_CONFIG,
           "Should reject invalid degraded dwell");
    
    PASS("Edge: Config validation");
}

TEST(edge_test_mode) {
    mode_manager_t m;
    mode_init(&m, NULL);
    
    /* Enter TEST mode */
    ASSERT(mode_enter_test(&m) == MODE_OK, "Should enter TEST");
    ASSERT(mode_get(&m) == MODE_TEST, "Should be in TEST");
    
    /* TEST mode allows full actions */
    ASSERT(mode_can_actuate(&m) == 1, "TEST allows actuation");
    
    /* Exit TEST */
    mode_exit_test(&m);
    ASSERT(mode_get(&m) == MODE_INIT, "Should return to INIT");
    
    PASS("Edge: TEST mode enter/exit");
}

TEST(edge_test_from_emergency) {
    mode_manager_t m;
    mode_config_t cfg = MODE_DEFAULT_CONFIG;
    cfg.min_dwell_startup = 1;
    mode_init(&m, &cfg);
    
    mode_input_t input = {0};
    mode_result_t r;
    
    /* Get to OPERATIONAL */
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        input.states[i] = HEALTH_HEALTHY;
    }
    for (int i = 0; i < 5; i++) {
        input.timestamp = i * 100;
        mode_update(&m, &input, &r);
    }
    
    /* Trigger EMERGENCY */
    input.states[0] = HEALTH_FAULTY;
    input.timestamp = 500;
    mode_update(&m, &input, &r);
    
    /* Cannot enter TEST from EMERGENCY */
    ASSERT(mode_enter_test(&m) == MODE_ERR_LOCKED,
           "Should not enter TEST from EMERGENCY");
    
    PASS("Edge: Cannot enter TEST from EMERGENCY");
}

/*===========================================================================
 * Fuzz Test
 *===========================================================================*/

TEST(fuzz_random_inputs) {
    mode_manager_t m;
    mode_init(&m, NULL);
    
    mode_input_t input;
    mode_result_t r;
    
    srand((unsigned)time(NULL));
    
    for (int i = 0; i < 10000; i++) {
        /* Random states */
        for (int j = 0; j < MODE_MODULE_COUNT; j++) {
            input.states[j] = (health_state_t)(rand() % 5);
        }
        
        /* Random flags */
        input.flags.approaching_upper = rand() % 2;
        input.flags.approaching_lower = rand() % 2;
        input.flags.low_confidence = rand() % 2;
        input.flags.queue_critical = rand() % 2;
        input.flags.timing_unstable = rand() % 2;
        input.flags.baseline_volatile = rand() % 2;
        
        input.timestamp = i;
        
        int err = mode_update(&m, &input, &r);
        
        ASSERT(err == MODE_OK, "Update should not fail");
        ASSERT(r.mode >= 0 && r.mode < MODE_COUNT, "Mode should be valid");
        
        /* Randomly reset sometimes */
        if (rand() % 100 == 0) {
            mode_reset(&m);
        }
    }
    
    PASS("Fuzz: 10000 random inputs, no crashes");
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║           MODE MANAGER Contract Test Suite                     ║\n");
    printf("║           Module 7: The Captain                                ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("Contract Tests:\n");
    RUN(contract_1_unambiguous_state);
    RUN(contract_2_safe_entry);
    RUN(contract_3_fault_stickiness);
    RUN(contract_4_no_skip);
    RUN(contract_5_bounded_latency);
    RUN(contract_6_deterministic);
    RUN(contract_7_proactive_safety);
    RUN(contract_8_auditability);
    
    printf("\nInvariant Tests:\n");
    RUN(invariant_1_mode_valid);
    RUN(invariant_2_operational_healthy);
    RUN(invariant_3_emergency_fault);
    RUN(invariant_4_dwell_monotonic);
    
    printf("\nEdge Case Tests:\n");
    RUN(edge_null_pointers);
    RUN(edge_config_validation);
    RUN(edge_test_mode);
    RUN(edge_test_from_emergency);
    
    printf("\nFuzz Tests:\n");
    RUN(fuzz_random_inputs);
    
    printf("\n");
    printf("══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);
    printf("══════════════════════════════════════════════════════════════════\n");
    printf("\n");
    
    return tests_failed > 0 ? 1 : 0;
}
