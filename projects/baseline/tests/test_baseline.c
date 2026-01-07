/**
 * test_baseline.c - Contract and Invariant Test Suite
 * 
 * This is not a unit test file. This is a proof harness.
 * Each test demonstrates a theorem, not just exercises an API.
 * 
 * Contract Tests:
 *   CONTRACT-1: Convergence
 *   CONTRACT-2: Sensitivity  
 *   CONTRACT-3: Stability
 *   CONTRACT-4: Spike Resistance
 * 
 * Invariant Tests:
 *   INV-1: State domain
 *   INV-2: Ready implies not learning
 *   INV-3: Fault implies deviation
 *   INV-5: Variance non-negative
 *   INV-7: Monotonic count
 * 
 * Fuzz Tests:
 *   Random streams
 *   NaN injection
 *   Edge cases
 * 
 * Copyright (c) 2025 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <time.h>
#include "baseline.h"

/*===========================================================================
 * Test Counters
 *===========================================================================*/

static int tests_run = 0;
static int tests_passed = 0;

#define TEST_PASS(name) do { \
    tests_run++; \
    tests_passed++; \
    printf("  [PASS] %s\n", name); \
} while(0)

#define TEST_FAIL(name, msg) do { \
    tests_run++; \
    printf("  [FAIL] %s: %s\n", name, msg); \
} while(0)

/*===========================================================================
 * CONTRACT TESTS
 *===========================================================================*/

/**
 * CONTRACT-1: Convergence
 * 
 * Test that baseline converges to true mean for stationary input.
 * Feed 1000 observations with known mean=100.
 * Baseline should converge to μ ≈ 100.
 */
static void test_contract1_convergence(void)
{
    base_fsm_t b;
    base_init(&b, &BASE_DEFAULT_CONFIG);
    
    /* Stationary input: mean=100, small alternating noise */
    for (int i = 0; i < 1000; i++) {
        double x = 100.0 + (i % 2 == 0 ? 0.5 : -0.5);
        base_step(&b, x);
    }
    
    /* Convergence check: μ should be very close to 100 */
    double error = fabs(b.mu - 100.0);
    
    if (error < 1.0) {
        printf("  [PASS] CONTRACT-1: Convergence (error=%.4f)\n", error);
        tests_run++;
        tests_passed++;
    } else {
        printf("  [FAIL] CONTRACT-1: Convergence (error=%.4f, expected < 1.0)\n", error);
        tests_run++;
    }
}

/**
 * CONTRACT-2: Sensitivity
 * 
 * Test that sustained deviation is detected.
 * Establish baseline at 100, inject large spike.
 * Should detect immediately.
 */
static void test_contract2_sensitivity(void)
{
    base_fsm_t b;
    base_config_t cfg = { 
        .alpha = 0.1, 
        .epsilon = 1e-9, 
        .k = 3.0, 
        .n_min = 30 
    };
    base_init(&b, &cfg);
    
    /* Establish tight baseline with many observations */
    for (int i = 0; i < 100; i++) {
        base_step(&b, 100.0 + (i % 2 == 0 ? 0.5 : -0.5));
    }
    
    if (b.state != BASE_STABLE) {
        TEST_FAIL("CONTRACT-2: Sensitivity", "Failed to reach STABLE");
        return;
    }
    
    /* Inject large deviation (should be many sigma) */
    double spike = 150.0;  /* ~66 sigma above mean */
    base_result_t r = base_step(&b, spike);
    
    if (r.state == BASE_DEVIATION && r.is_deviation == 1) {
        printf("  [PASS] CONTRACT-2: Sensitivity (z=%.2f, detected immediately)\n", r.z);
        tests_run++;
        tests_passed++;
    } else {
        printf("  [FAIL] CONTRACT-2: Sensitivity (state=%d, expected DEVIATION)\n", r.state);
        tests_run++;
    }
}

/**
 * CONTRACT-3: Stability
 * 
 * Test that normal fluctuations don't trigger false positives.
 * Feed observations within normal range (< k sigma).
 * Should remain STABLE, never hit DEVIATION.
 */
static void test_contract3_stability(void)
{
    base_fsm_t b;
    base_init(&b, &BASE_DEFAULT_CONFIG);
    
    /* Learning phase with consistent variance */
    for (int i = 0; i < 100; i++) {
        base_step(&b, 100.0 + (i % 2 == 0 ? 1.0 : -1.0));
    }
    
    if (b.state != BASE_STABLE) {
        TEST_FAIL("CONTRACT-3: Stability", "Failed to reach STABLE");
        return;
    }
    
    /* Normal operation: small fluctuations within learned variance */
    int false_positives = 0;
    for (int i = 0; i < 1000; i++) {
        double x = 100.0 + (i % 3 - 1) * 0.5;  /* 99.5, 100.0, 100.5 */
        base_result_t r = base_step(&b, x);
        if (r.state == BASE_DEVIATION) {
            false_positives++;
        }
    }
    
    if (false_positives == 0) {
        TEST_PASS("CONTRACT-3: Stability (0 false positives in 1000)");
    } else {
        printf("  [FAIL] CONTRACT-3: Stability (%d false positives)\n", false_positives);
        tests_run++;
    }
}

/**
 * CONTRACT-4: Spike Resistance
 * 
 * Test that single outlier shifts mean by at most α·M.
 * This is the critical safety property.
 */
static void test_contract4_spike_resistance(void)
{
    base_fsm_t b;
    base_init(&b, &BASE_DEFAULT_CONFIG);
    
    /* Establish baseline at exactly 100.0 (constant input) */
    for (int i = 0; i < 100; i++) {
        base_step(&b, 100.0);
    }
    double mu_before = b.mu;
    
    /* Inject massive spike */
    double spike = 1000.0;
    base_result_t r = base_step(&b, spike);
    double mu_after = b.mu;
    
    /* Check bounded shift: Δμ ≤ α·(spike - mu_before) */
    double actual_shift = mu_after - mu_before;
    double max_allowed = b.cfg.alpha * (spike - mu_before);
    
    /* r.deviation should equal (spike - mu_before) */
    double expected_deviation = spike - mu_before;
    
    if (actual_shift <= max_allowed + 1e-9 && 
        fabs(r.deviation - expected_deviation) < 1e-9) {
        printf("  [PASS] CONTRACT-4: Spike Resistance\n");
        printf("         Spike=%.0f, Shift=%.2f, Max=%.2f\n",
               spike, actual_shift, max_allowed);
        tests_run++;
        tests_passed++;
    } else {
        printf("  [FAIL] CONTRACT-4: Spike Resistance\n");
        printf("         Shift=%.2f exceeded Max=%.2f\n", 
               actual_shift, max_allowed);
        tests_run++;
    }
}

/*===========================================================================
 * INVARIANT TESTS
 *===========================================================================*/

/**
 * INV-1: State Domain
 * 
 * state ∈ { LEARNING, STABLE, DEVIATION }
 */
static void test_inv1_state_domain(void)
{
    base_fsm_t b;
    base_init(&b, &BASE_DEFAULT_CONFIG);
    
    srand((unsigned)time(NULL));
    
    for (int i = 0; i < 1000; i++) {
        double x = (double)(rand() % 1000);
        base_step(&b, x);
        
        if (!(b.state == BASE_LEARNING ||
              b.state == BASE_STABLE ||
              b.state == BASE_DEVIATION)) {
            TEST_FAIL("INV-1: State domain", "Invalid state value");
            return;
        }
    }
    
    TEST_PASS("INV-1: State always in valid domain");
}

/**
 * INV-2: Ready Implies Not Learning
 * 
 * (state ≠ LEARNING) → (n ≥ n_min ∧ variance > epsilon)
 */
static void test_inv2_ready_implies_not_learning(void)
{
    base_fsm_t b;
    base_init(&b, &BASE_DEFAULT_CONFIG);
    
    for (int i = 0; i < 100; i++) {
        base_step(&b, 100.0 + (double)(rand() % 10));
        
        if (b.state != BASE_LEARNING) {
            if (b.n < b.cfg.n_min || b.variance <= b.cfg.epsilon) {
                TEST_FAIL("INV-2: Ready implies not learning", 
                         "State is not LEARNING but conditions not met");
                return;
            }
        }
    }
    
    TEST_PASS("INV-2: Ready implies sufficient evidence");
}

/**
 * INV-3: Fault Implies Deviation
 * 
 * (fault_fp ∨ fault_reentry) → (state == DEVIATION)
 */
static void test_inv3_fault_implies_deviation(void)
{
    base_fsm_t b;
    base_init(&b, &BASE_DEFAULT_CONFIG);
    
    /* Some normal observations first */
    for (int i = 0; i < 5; i++) {
        base_step(&b, 100.0);
    }
    
    /* Inject NaN */
    base_step(&b, 0.0 / 0.0);
    
    if (base_faulted(&b) && b.state == BASE_DEVIATION) {
        TEST_PASS("INV-3: Fault implies DEVIATION");
    } else {
        TEST_FAIL("INV-3: Fault implies DEVIATION",
                 "Faulted but not in DEVIATION state");
    }
}

/**
 * INV-5: Variance Non-Negative
 * 
 * variance ≥ 0
 */
static void test_inv5_variance_nonnegative(void)
{
    base_fsm_t b;
    base_init(&b, &BASE_DEFAULT_CONFIG);
    
    /* Random observations including negatives */
    for (int i = 0; i < 10000; i++) {
        double x = (double)(rand() % 2000) - 1000.0;
        base_step(&b, x);
        
        if (b.variance < 0.0) {
            TEST_FAIL("INV-5: Variance non-negative", "Negative variance");
            return;
        }
    }
    
    TEST_PASS("INV-5: Variance always non-negative");
}

/**
 * INV-7: Monotonic Count
 * 
 * n increments monotonically on non-faulted steps
 */
static void test_inv7_monotonic_count(void)
{
    base_fsm_t b;
    base_init(&b, &BASE_DEFAULT_CONFIG);
    
    uint32_t prev_n = 0;
    for (int i = 0; i < 1000; i++) {
        base_step(&b, 100.0);
        
        if (b.n != prev_n + 1) {
            TEST_FAIL("INV-7: Monotonic count", "Count did not increment");
            return;
        }
        prev_n = b.n;
    }
    
    TEST_PASS("INV-7: Count monotonically increasing");
}

/**
 * INV-7 (fault case): Faulted input does NOT increment n
 */
static void test_inv7_fault_no_increment(void)
{
    base_fsm_t b;
    base_init(&b, &BASE_DEFAULT_CONFIG);
    
    /* Some normal observations */
    for (int i = 0; i < 10; i++) {
        base_step(&b, 100.0);
    }
    uint32_t n_before = b.n;
    
    /* Inject NaN */
    base_step(&b, 0.0 / 0.0);
    
    if (b.n == n_before) {
        TEST_PASS("INV-7: Faulted input does not increment n");
    } else {
        TEST_FAIL("INV-7: Faulted input does not increment n",
                 "n was incremented on fault");
    }
}

/*===========================================================================
 * FUZZ TESTS
 *===========================================================================*/

/**
 * Fuzz: Random Streams
 * 
 * 100,000 random observations, all invariants must hold.
 */
static void test_fuzz_random_streams(void)
{
    base_fsm_t b;
    base_init(&b, &BASE_DEFAULT_CONFIG);
    
    for (int i = 0; i < 100000; i++) {
        double x = (double)(rand()) / RAND_MAX * 1000.0;
        base_result_t r = base_step(&b, x);
        
        /* INV-1: State domain */
        if (!(b.state == BASE_LEARNING ||
              b.state == BASE_STABLE ||
              b.state == BASE_DEVIATION)) {
            TEST_FAIL("Fuzz: Random streams", "Invalid state");
            return;
        }
        
        /* INV-5: Variance non-negative */
        if (b.variance < 0.0) {
            TEST_FAIL("Fuzz: Random streams", "Negative variance");
            return;
        }
        
        /* is_deviation consistency */
        if ((r.is_deviation == 1) != (r.state == BASE_DEVIATION)) {
            TEST_FAIL("Fuzz: Random streams", "is_deviation inconsistent");
            return;
        }
    }
    
    TEST_PASS("Fuzz: 100000 random observations, invariants held");
}

/**
 * Fuzz: NaN Injection
 * 
 * 1% random NaN injection, system must remain consistent.
 */
static void test_fuzz_nan_injection(void)
{
    base_fsm_t b;
    base_init(&b, &BASE_DEFAULT_CONFIG);
    
    int nan_count = 0;
    for (int i = 0; i < 10000; i++) {
        double x;
        if (rand() % 100 == 0) {
            x = 0.0 / 0.0;  /* 1% NaN injection */
            nan_count++;
        } else {
            x = 100.0 + (double)(rand() % 10);
        }
        
        base_step(&b, x);
        
        /* State must always be valid */
        if (!(b.state == BASE_LEARNING ||
              b.state == BASE_STABLE ||
              b.state == BASE_DEVIATION)) {
            TEST_FAIL("Fuzz: NaN injection", "Invalid state after NaN");
            return;
        }
        
        /* INV-3: If faulted, must be in DEVIATION */
        if (base_faulted(&b) && b.state != BASE_DEVIATION) {
            TEST_FAIL("Fuzz: NaN injection", "Faulted but not DEVIATION");
            return;
        }
    }
    
    printf("  [PASS] Fuzz: NaN injection (%d NaNs injected, handled safely)\n", nan_count);
    tests_run++;
    tests_passed++;
}

/**
 * Fuzz: Infinity Injection
 */
static void test_fuzz_inf_injection(void)
{
    base_fsm_t b;
    base_init(&b, &BASE_DEFAULT_CONFIG);
    
    /* Some normal observations */
    for (int i = 0; i < 10; i++) {
        base_step(&b, 100.0);
    }
    
    /* Inject +Inf */
    base_step(&b, 1.0 / 0.0);
    
    if (base_faulted(&b) && b.state == BASE_DEVIATION) {
        TEST_PASS("Fuzz: +Inf handled (fault_fp set)");
    } else {
        TEST_FAIL("Fuzz: +Inf handling", "Did not fault on +Inf");
    }
    
    /* Reset and test -Inf */
    base_reset(&b);
    for (int i = 0; i < 10; i++) {
        base_step(&b, 100.0);
    }
    
    base_step(&b, -1.0 / 0.0);
    
    if (base_faulted(&b) && b.state == BASE_DEVIATION) {
        TEST_PASS("Fuzz: -Inf handled (fault_fp set)");
    } else {
        TEST_FAIL("Fuzz: -Inf handling", "Did not fault on -Inf");
    }
}

/*===========================================================================
 * EDGE CASE TESTS
 *===========================================================================*/

/**
 * Edge: Zero Variance
 * 
 * Constant input produces zero variance. System must handle gracefully.
 */
static void test_edge_zero_variance(void)
{
    base_fsm_t b;
    base_init(&b, &BASE_DEFAULT_CONFIG);
    
    /* Constant input = zero variance */
    for (int i = 0; i < 100; i++) {
        base_result_t r = base_step(&b, 100.0);
        
        /* z should be 0 when variance <= epsilon (variance floor) */
        if (b.variance <= b.cfg.epsilon && r.z != 0.0) {
            TEST_FAIL("Edge: Zero variance", "z != 0 when variance <= epsilon");
            return;
        }
    }
    
    TEST_PASS("Edge: Zero variance handled (z=0 when var<=eps)");
}

/**
 * Edge: Extreme Values
 * 
 * Very large finite values should not cause faults.
 */
static void test_edge_extreme_values(void)
{
    base_fsm_t b;
    base_init(&b, &BASE_DEFAULT_CONFIG);
    
    /* Very large values */
    base_step(&b, 1e15);
    base_step(&b, -1e15);
    base_step(&b, 1e-15);
    
    if (!base_faulted(&b)) {
        TEST_PASS("Edge: Extreme finite values handled");
    } else {
        TEST_FAIL("Edge: Extreme values", "Faulted on finite values");
    }
}

/**
 * Edge: Config Validation
 * 
 * Invalid configs should be rejected.
 */
static void test_edge_config_validation(void)
{
    base_fsm_t b;
    int result;
    
    /* Alpha out of range */
    base_config_t bad_alpha = { .alpha = 0.0, .epsilon = 1e-9, .k = 3.0, .n_min = 20 };
    result = base_init(&b, &bad_alpha);
    if (result != -1) {
        TEST_FAIL("Edge: Config validation", "Accepted alpha=0");
        return;
    }
    
    bad_alpha.alpha = 1.0;
    result = base_init(&b, &bad_alpha);
    if (result != -1) {
        TEST_FAIL("Edge: Config validation", "Accepted alpha=1");
        return;
    }
    
    /* n_min too small for alpha */
    base_config_t bad_nmin = { .alpha = 0.01, .epsilon = 1e-9, .k = 3.0, .n_min = 1 };
    result = base_init(&b, &bad_nmin);
    if (result != -1) {
        TEST_FAIL("Edge: Config validation", "Accepted n_min < ceil(2/alpha)");
        return;
    }
    
    /* Epsilon zero */
    base_config_t bad_eps = { .alpha = 0.1, .epsilon = 0.0, .k = 3.0, .n_min = 20 };
    result = base_init(&b, &bad_eps);
    if (result != -1) {
        TEST_FAIL("Edge: Config validation", "Accepted epsilon=0");
        return;
    }
    
    TEST_PASS("Edge: Config validation rejects invalid params");
}

/**
 * Edge: Reset Clears Faults
 * 
 * Sticky faults should be cleared by base_reset().
 */
static void test_edge_reset_clears_faults(void)
{
    base_fsm_t b;
    base_init(&b, &BASE_DEFAULT_CONFIG);
    
    /* Inject fault */
    base_step(&b, 0.0 / 0.0);
    
    if (!base_faulted(&b)) {
        TEST_FAIL("Edge: Reset clears faults", "Fault not set");
        return;
    }
    
    /* Reset */
    base_reset(&b);
    
    if (base_faulted(&b)) {
        TEST_FAIL("Edge: Reset clears faults", "Fault not cleared");
        return;
    }
    
    if (b.state != BASE_LEARNING) {
        TEST_FAIL("Edge: Reset clears faults", "State not LEARNING after reset");
        return;
    }
    
    if (b.n != 0) {
        TEST_FAIL("Edge: Reset clears faults", "n not zeroed after reset");
        return;
    }
    
    TEST_PASS("Edge: Reset clears faults and state");
}

/*===========================================================================
 * MAIN
 *===========================================================================*/

int main(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║              Baseline Contract Test Suite                      ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("Contract Tests:\n");
    test_contract1_convergence();
    test_contract2_sensitivity();
    test_contract3_stability();
    test_contract4_spike_resistance();
    printf("\n");
    
    printf("Invariant Tests:\n");
    test_inv1_state_domain();
    test_inv2_ready_implies_not_learning();
    test_inv3_fault_implies_deviation();
    test_inv5_variance_nonnegative();
    test_inv7_monotonic_count();
    test_inv7_fault_no_increment();
    printf("\n");
    
    printf("Fuzz Tests:\n");
    test_fuzz_random_streams();
    test_fuzz_nan_injection();
    test_fuzz_inf_injection();
    printf("\n");
    
    printf("Edge Case Tests:\n");
    test_edge_zero_variance();
    test_edge_extreme_values();
    test_edge_config_validation();
    test_edge_reset_clears_faults();
    printf("\n");
    
    printf("══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("══════════════════════════════════════════════════════════════════\n");
    printf("\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
