/**
 * test_contracts.c - Contract verification for pulse
 * 
 * Tests verify the three formal contracts:
 *   CONTRACT-1: Soundness  - Never report ALIVE if actually dead
 *   CONTRACT-2: Liveness   - Eventually report DEAD if heartbeats stop
 *   CONTRACT-3: Stability  - No spurious transitions
 * 
 * Copyright (c) 2025 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include "pulse.h"

/*---------------------------------------------------------------------------
 * Test Counters
 *---------------------------------------------------------------------------*/
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  [TEST] %s... ", name); \
    fflush(stdout); \
} while(0)

#define PASS() do { \
    tests_passed++; \
    printf("PASS\n"); \
} while(0)

/*---------------------------------------------------------------------------
 * Invariant Verification
 *---------------------------------------------------------------------------*/
static void verify_invariants(const hb_fsm_t *m)
{
    /* INV-1: Valid state */
    assert(m->st == STATE_UNKNOWN || 
           m->st == STATE_ALIVE || 
           m->st == STATE_DEAD);
    
    /* INV-2: ALIVE requires evidence */
    if (m->st == STATE_ALIVE) {
        assert(m->have_hb == 1);
    }
    
    /* INV-3: Fault implies DEAD */
    if (m->fault_time || m->fault_reentry) {
        assert(m->st == STATE_DEAD);
    }
    
    /* INV-4: Not in step after return */
    assert(m->in_step == 0);
}

/*---------------------------------------------------------------------------
 * CONTRACT-1: Soundness Tests
 *---------------------------------------------------------------------------*/
static void test_contract1_basic(void)
{
    TEST("CONTRACT-1: No ALIVE after timeout");
    
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    hb_init(&m, 0);
    
    /* Send one heartbeat at t=0 */
    hb_step(&m, 0, 1, T, W);
    assert(hb_state(&m) == STATE_ALIVE);
    
    /* Advance past timeout without heartbeat */
    hb_step(&m, T + 1, 0, T, W);
    
    /* CONTRACT-1: Must NOT be ALIVE */
    assert(hb_state(&m) != STATE_ALIVE);
    assert(hb_state(&m) == STATE_DEAD);
    verify_invariants(&m);
    
    PASS();
}

static void test_contract1_no_evidence(void)
{
    TEST("CONTRACT-1: No ALIVE without evidence");
    
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    hb_init(&m, 0);
    
    /* Never send heartbeat, advance time */
    for (uint64_t t = 0; t <= 5000; t += 100) {
        hb_step(&m, t, 0, T, W);
        /* Must never be ALIVE without evidence */
        assert(hb_state(&m) != STATE_ALIVE);
        verify_invariants(&m);
    }
    
    PASS();
}

/*---------------------------------------------------------------------------
 * CONTRACT-2: Liveness Tests
 *---------------------------------------------------------------------------*/
static void test_contract2_basic(void)
{
    TEST("CONTRACT-2: DEAD reached after timeout");
    
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    hb_init(&m, 0);
    
    /* Send one heartbeat */
    hb_step(&m, 0, 1, T, W);
    assert(hb_state(&m) == STATE_ALIVE);
    
    /* Simulate time passing, no heartbeats */
    int reached_dead = 0;
    for (uint64_t t = 100; t <= T + 500; t += 100) {
        hb_step(&m, t, 0, T, W);
        if (hb_state(&m) == STATE_DEAD) {
            reached_dead = 1;
            break;
        }
    }
    
    /* CONTRACT-2: Must reach DEAD */
    assert(reached_dead);
    verify_invariants(&m);
    
    PASS();
}

static void test_contract2_timing(void)
{
    TEST("CONTRACT-2: DEAD at exactly T+1");
    
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    hb_init(&m, 0);
    hb_step(&m, 0, 1, T, W);
    
    /* At T, should still be ALIVE */
    hb_step(&m, T, 0, T, W);
    assert(hb_state(&m) == STATE_ALIVE);
    
    /* At T+1, should be DEAD */
    hb_step(&m, T + 1, 0, T, W);
    assert(hb_state(&m) == STATE_DEAD);
    verify_invariants(&m);
    
    PASS();
}

/*---------------------------------------------------------------------------
 * CONTRACT-3: Stability Tests
 *---------------------------------------------------------------------------*/
static void test_contract3_steady_heartbeats(void)
{
    TEST("CONTRACT-3: No spurious DEAD with steady heartbeats");
    
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    hb_init(&m, 0);
    
    /* Send heartbeats regularly */
    for (uint64_t t = 0; t <= 10000; t += 100) {
        hb_step(&m, t, 1, T, W);
        /* Should always be ALIVE with regular heartbeats */
        assert(hb_state(&m) == STATE_ALIVE);
        verify_invariants(&m);
    }
    
    PASS();
}

static void test_contract3_recovery(void)
{
    TEST("CONTRACT-3: Recovery from DEAD");
    
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    hb_init(&m, 0);
    hb_step(&m, 0, 1, T, W);
    
    /* Let it go DEAD */
    hb_step(&m, T + 1, 0, T, W);
    assert(hb_state(&m) == STATE_DEAD);
    
    /* Send heartbeat - should recover */
    hb_step(&m, T + 2, 1, T, W);
    assert(hb_state(&m) == STATE_ALIVE);
    verify_invariants(&m);
    
    PASS();
}

/*---------------------------------------------------------------------------
 * Boundary Tests
 *---------------------------------------------------------------------------*/
static void test_boundary_T_minus_1(void)
{
    TEST("Boundary: ALIVE at T-1");
    
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    hb_init(&m, 0);
    hb_step(&m, 0, 1, T, W);
    hb_step(&m, T - 1, 0, T, W);
    
    assert(hb_state(&m) == STATE_ALIVE);
    verify_invariants(&m);
    
    PASS();
}

static void test_boundary_exactly_T(void)
{
    TEST("Boundary: ALIVE at exactly T");
    
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    hb_init(&m, 0);
    hb_step(&m, 0, 1, T, W);
    hb_step(&m, T, 0, T, W);
    
    /* age = T, condition is a_hb > T, so T is NOT > T */
    assert(hb_state(&m) == STATE_ALIVE);
    verify_invariants(&m);
    
    PASS();
}

static void test_boundary_T_plus_1(void)
{
    TEST("Boundary: DEAD at T+1");
    
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    hb_init(&m, 0);
    hb_step(&m, 0, 1, T, W);
    hb_step(&m, T + 1, 0, T, W);
    
    assert(hb_state(&m) == STATE_DEAD);
    verify_invariants(&m);
    
    PASS();
}

/*---------------------------------------------------------------------------
 * Fault Injection Tests
 *---------------------------------------------------------------------------*/
static void test_fault_clock_backward(void)
{
    TEST("Fault: Clock jump backward");
    
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    hb_init(&m, 1000);
    hb_step(&m, 1000, 1, T, W);
    assert(hb_state(&m) == STATE_ALIVE);
    
    /* Clock jumps backward: now < last_hb by huge amount */
    /* This makes age wrap to a huge value (> 2^63) */
    hb_step(&m, 500, 0, T, W);
    
    /* Should trigger fault and DEAD */
    assert(hb_state(&m) == STATE_DEAD);
    assert(hb_faulted(&m) == 1);
    assert(m.fault_time == 1);
    verify_invariants(&m);
    
    PASS();
}

static void test_fault_reentry(void)
{
    TEST("Fault: Reentrancy detection");
    
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    hb_init(&m, 0);
    hb_step(&m, 0, 1, T, W);
    assert(hb_state(&m) == STATE_ALIVE);
    
    /* Simulate reentrancy by manually setting in_step */
    m.in_step = 1;
    hb_step(&m, 100, 1, T, W);
    
    /* Should detect reentry and fail safe */
    assert(hb_state(&m) == STATE_DEAD);
    assert(hb_faulted(&m) == 1);
    assert(m.fault_reentry == 1);
    
    PASS();
}

/*---------------------------------------------------------------------------
 * Invariant Tests
 *---------------------------------------------------------------------------*/
static void test_invariants_throughout(void)
{
    TEST("Invariants: Hold through state transitions");
    
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    
    /* After init */
    hb_init(&m, 0);
    verify_invariants(&m);
    
    /* After first heartbeat */
    hb_step(&m, 0, 1, T, W);
    verify_invariants(&m);
    
    /* During ALIVE */
    hb_step(&m, 500, 0, T, W);
    verify_invariants(&m);
    
    /* Transition to DEAD */
    hb_step(&m, T + 1, 0, T, W);
    verify_invariants(&m);
    
    /* Recovery */
    hb_step(&m, T + 2, 1, T, W);
    verify_invariants(&m);
    
    PASS();
}

/*---------------------------------------------------------------------------
 * Fuzz Test
 *---------------------------------------------------------------------------*/
static void test_fuzz(uint64_t iterations)
{
    char name[64];
    snprintf(name, sizeof(name), "Fuzz: %lu random iterations", 
             (unsigned long)iterations);
    TEST(name);
    
    hb_fsm_t m;
    uint64_t T = 1000;
    uint64_t W = 0;
    uint64_t now = 0;
    
    srand((unsigned int)time(NULL));
    
    hb_init(&m, 0);
    
    for (uint64_t i = 0; i < iterations; i++) {
        /* Random heartbeat (0 or 1) */
        uint8_t hb = (uint8_t)(rand() % 2);
        
        /* Random time advance (1-500) */
        now += (uint64_t)(1 + rand() % 500);
        
        hb_step(&m, now, hb, T, W);
        
        /* Verify invariants after every step */
        verify_invariants(&m);
        
        /* Verify CONTRACT-1: If ALIVE, evidence must be fresh */
        if (hb_state(&m) == STATE_ALIVE) {
            uint64_t age = now - m.last_hb;
            assert(age <= T);
            assert(m.have_hb == 1);
        }
    }
    
    PASS();
}

/*---------------------------------------------------------------------------
 * Main
 *---------------------------------------------------------------------------*/
int main(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║              Pulse Contract Test Suite                         ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("CONTRACT-1 (Soundness) Tests:\n");
    test_contract1_basic();
    test_contract1_no_evidence();
    
    printf("\nCONTRACT-2 (Liveness) Tests:\n");
    test_contract2_basic();
    test_contract2_timing();
    
    printf("\nCONTRACT-3 (Stability) Tests:\n");
    test_contract3_steady_heartbeats();
    test_contract3_recovery();
    
    printf("\nBoundary Tests:\n");
    test_boundary_T_minus_1();
    test_boundary_exactly_T();
    test_boundary_T_plus_1();
    
    printf("\nFault Injection Tests:\n");
    test_fault_clock_backward();
    test_fault_reentry();
    
    printf("\nInvariant Tests:\n");
    test_invariants_throughout();
    
    printf("\nFuzz Tests:\n");
    test_fuzz(100000);
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║  Results: %d/%d tests passed                                    ║\n", 
           tests_passed, tests_run);
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
