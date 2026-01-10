/**
 * test_pressure.c - Contract and Invariant Test Suite
 * 
 * This is a proof harness for the pressure queue module.
 * 
 * Contract Tests:
 *   CONTRACT-1: Bounded memory
 *   CONTRACT-2: No data loss (full accounting)
 *   CONTRACT-3: FIFO ordering
 *   CONTRACT-4: Pressure signal accuracy
 * 
 * Invariant Tests:
 *   INV-1: count <= capacity
 *   INV-2: head, tail < capacity
 *   INV-3: Accounting equation
 * 
 * Policy Tests:
 *   REJECT, DROP_OLDEST, DROP_NEWEST
 * 
 * Copyright (c) 2026 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pressure.h"

/*===========================================================================
 * Test Framework
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

#define ASSERT_TRUE(cond, name, msg) do { \
    if (!(cond)) { TEST_FAIL(name, msg); return; } \
} while(0)

#define ASSERT_EQ(a, b, name, msg) do { \
    if ((a) != (b)) { TEST_FAIL(name, msg); return; } \
} while(0)

/*===========================================================================
 * CONTRACT TESTS
 *===========================================================================*/

/**
 * CONTRACT-1: Bounded Memory
 */
static void test_contract1_bounded_memory(void)
{
    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 16;
    cfg.policy = POLICY_DROP_OLDEST;
    cfg.high_water = 12;
    cfg.low_water = 4;
    cfg.critical_water = 15;
    pressure_item_t buffer[16];

    pressure_init(&q, &cfg, buffer);

    /* Enqueue way more than capacity */
    for (uint64_t i = 0; i < 1000; i++) {
        pressure_enqueue(&q, i, i, NULL);
        ASSERT_TRUE(q.count <= cfg.capacity, "CONTRACT-1",
                   "Count exceeded capacity");
    }

    TEST_PASS("CONTRACT-1: Bounded memory (count <= capacity after 1000 enqueues)");
}

/**
 * CONTRACT-1b: Bounded with REJECT policy
 */
static void test_contract1b_bounded_reject(void)
{
    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 8;
    cfg.policy = POLICY_REJECT;
    cfg.high_water = 6;
    cfg.low_water = 2;
    cfg.critical_water = 7;
    pressure_item_t buffer[8];

    pressure_init(&q, &cfg, buffer);

    int rejected = 0;
    for (uint64_t i = 0; i < 100; i++) {
        int err = pressure_enqueue(&q, i, i, NULL);
        if (err == PRESSURE_ERR_FULL) rejected++;
        ASSERT_TRUE(q.count <= cfg.capacity, "CONTRACT-1b",
                   "Count exceeded capacity");
    }

    ASSERT_TRUE(rejected == 92, "CONTRACT-1b", "Wrong rejection count");
    TEST_PASS("CONTRACT-1b: Bounded memory with REJECT (92 rejections)");
}

/**
 * CONTRACT-2: No Data Loss (Full Accounting)
 */
static void test_contract2_accounting(void)
{
    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 10;
    cfg.policy = POLICY_DROP_OLDEST;
    cfg.high_water = 8;
    cfg.low_water = 3;
    cfg.critical_water = 9;
    pressure_item_t buffer[10];

    pressure_init(&q, &cfg, buffer);

    /* Mix of enqueues and dequeues */
    for (uint64_t i = 0; i < 50; i++) {
        pressure_enqueue(&q, i, i, NULL);
    }

    pressure_item_t item;
    for (int i = 0; i < 7; i++) {
        pressure_dequeue(&q, &item, NULL);
    }

    pressure_stats_t s;
    pressure_get_stats(&q, &s);

    uint64_t total_dropped = s.dropped_oldest + s.dropped_newest + s.rejected;
    uint64_t accounted = s.dequeued + q.count + total_dropped;

    ASSERT_EQ(s.enqueued, accounted, "CONTRACT-2",
             "Accounting mismatch: enqueued != dequeued + in_queue + dropped");

    TEST_PASS("CONTRACT-2: No data loss (accounting equation holds)");
}

/**
 * CONTRACT-3: FIFO Ordering
 */
static void test_contract3_fifo(void)
{
    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 32;
    cfg.policy = POLICY_REJECT;
    cfg.high_water = 24;
    cfg.low_water = 8;
    cfg.critical_water = 30;
    pressure_item_t buffer[32];

    pressure_init(&q, &cfg, buffer);

    /* Enqueue 20 items */
    for (uint64_t i = 0; i < 20; i++) {
        pressure_enqueue(&q, i * 100, i, NULL);
    }

    /* Dequeue and verify order */
    pressure_item_t item;
    uint64_t expected = 0;
    while (pressure_dequeue(&q, &item, NULL) == PRESSURE_OK) {
        ASSERT_EQ(item.payload, expected * 100, "CONTRACT-3",
                 "FIFO order violated");
        expected++;
    }

    ASSERT_EQ(expected, 20, "CONTRACT-3", "Wrong number of items dequeued");
    TEST_PASS("CONTRACT-3: FIFO ordering preserved");
}

/**
 * CONTRACT-3b: FIFO with wraparound
 */
static void test_contract3b_fifo_wraparound(void)
{
    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 8;
    cfg.policy = POLICY_REJECT;
    cfg.high_water = 6;
    cfg.low_water = 2;
    cfg.critical_water = 7;
    pressure_item_t buffer[8];

    pressure_init(&q, &cfg, buffer);

    /* Cause wraparound by enqueue/dequeue cycles */
    pressure_item_t item;
    for (int cycle = 0; cycle < 5; cycle++) {
        /* Enqueue 5 */
        for (uint64_t i = 0; i < 5; i++) {
            pressure_enqueue(&q, cycle * 100 + i, 0, NULL);
        }
        /* Dequeue 3 */
        for (int i = 0; i < 3; i++) {
            pressure_dequeue(&q, &item, NULL);
        }
    }

    /* Now verify remaining items are in order */
    uint64_t last = 0;
    int first = 1;
    while (pressure_dequeue(&q, &item, NULL) == PRESSURE_OK) {
        if (!first) {
            ASSERT_TRUE(item.payload > last, "CONTRACT-3b",
                       "FIFO order violated after wraparound");
        }
        last = item.payload;
        first = 0;
    }

    TEST_PASS("CONTRACT-3b: FIFO preserved through wraparound");
}

/**
 * CONTRACT-4: Pressure Signal Accuracy
 */
static void test_contract4_pressure_signal(void)
{
    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 100;
    cfg.policy = POLICY_REJECT;
    cfg.low_water = 25;
    cfg.high_water = 75;
    cfg.critical_water = 90;
    pressure_item_t buffer[100];

    pressure_init(&q, &cfg, buffer);

    /* Fill to various levels and check state */
    pressure_result_t r;

    /* Fill to 10 (LOW) */
    for (uint64_t i = 0; i < 10; i++) {
        pressure_enqueue(&q, i, i, &r);
    }
    ASSERT_EQ(r.state, PRESSURE_LOW, "CONTRACT-4", "Should be LOW at 10%");

    /* Fill to 50 (NORMAL) */
    for (uint64_t i = 10; i < 50; i++) {
        pressure_enqueue(&q, i, i, &r);
    }
    ASSERT_EQ(r.state, PRESSURE_NORMAL, "CONTRACT-4", "Should be NORMAL at 50%");

    /* Fill to 80 (HIGH) */
    for (uint64_t i = 50; i < 80; i++) {
        pressure_enqueue(&q, i, i, &r);
    }
    ASSERT_EQ(r.state, PRESSURE_HIGH, "CONTRACT-4", "Should be HIGH at 80%");

    /* Fill to 95 (CRITICAL) */
    for (uint64_t i = 80; i < 95; i++) {
        pressure_enqueue(&q, i, i, &r);
    }
    ASSERT_EQ(r.state, PRESSURE_CRITICAL, "CONTRACT-4", "Should be CRITICAL at 95%");

    TEST_PASS("CONTRACT-4: Pressure signal accuracy");
}

/*===========================================================================
 * INVARIANT TESTS
 *===========================================================================*/

/**
 * INV-1: count <= capacity
 */
static void test_inv1_count_bound(void)
{
    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 16;
    cfg.policy = POLICY_DROP_OLDEST;
    cfg.high_water = 12;
    cfg.low_water = 4;
    cfg.critical_water = 15;
    pressure_item_t buffer[16];

    pressure_init(&q, &cfg, buffer);

    srand(42);
    for (int i = 0; i < 10000; i++) {
        if (rand() % 3 != 0) {
            pressure_enqueue(&q, i, i, NULL);
        } else {
            pressure_item_t item;
            pressure_dequeue(&q, &item, NULL);
        }
        ASSERT_TRUE(q.count <= q.cfg.capacity, "INV-1",
                   "Count exceeded capacity");
    }

    TEST_PASS("INV-1: count <= capacity (10000 random ops)");
}

/**
 * INV-2: head, tail < capacity
 */
static void test_inv2_index_bounds(void)
{
    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 8;
    cfg.policy = POLICY_DROP_OLDEST;
    cfg.high_water = 6;
    cfg.low_water = 2;
    cfg.critical_water = 7;
    pressure_item_t buffer[8];

    pressure_init(&q, &cfg, buffer);

    for (int i = 0; i < 1000; i++) {
        pressure_enqueue(&q, i, i, NULL);
        ASSERT_TRUE(q.head < q.cfg.capacity, "INV-2", "head >= capacity");
        ASSERT_TRUE(q.tail < q.cfg.capacity, "INV-2", "tail >= capacity");

        pressure_item_t item;
        if (i % 2 == 0) {
            pressure_dequeue(&q, &item, NULL);
            ASSERT_TRUE(q.head < q.cfg.capacity, "INV-2", "head >= capacity after dequeue");
        }
    }

    TEST_PASS("INV-2: head, tail always < capacity");
}

/**
 * INV-3: Sequence numbers are monotonic
 */
static void test_inv3_sequence_monotonic(void)
{
    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 16;
    cfg.policy = POLICY_REJECT;
    cfg.high_water = 12;
    cfg.low_water = 4;
    cfg.critical_water = 15;
    pressure_item_t buffer[16];

    pressure_init(&q, &cfg, buffer);

    for (uint64_t i = 0; i < 10; i++) {
        pressure_enqueue(&q, i, i, NULL);
    }

    pressure_item_t item;
    uint32_t last_seq = 0;
    while (pressure_dequeue(&q, &item, NULL) == PRESSURE_OK) {
        ASSERT_TRUE(item.sequence > last_seq, "INV-3",
                   "Sequence not monotonic");
        last_seq = item.sequence;
    }

    TEST_PASS("INV-3: Sequence numbers are monotonic");
}

/*===========================================================================
 * POLICY TESTS
 *===========================================================================*/

/**
 * REJECT policy test
 */
static void test_policy_reject(void)
{
    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 4;
    cfg.policy = POLICY_REJECT;
    cfg.high_water = 3;
    cfg.low_water = 1;
    cfg.critical_water = 4;
    pressure_item_t buffer[4];

    pressure_init(&q, &cfg, buffer);

    /* Fill queue */
    for (int i = 0; i < 4; i++) {
        int err = pressure_enqueue(&q, i, i, NULL);
        ASSERT_EQ(err, PRESSURE_OK, "REJECT", "Should accept first 4");
    }

    /* Next should be rejected */
    int err = pressure_enqueue(&q, 99, 99, NULL);
    ASSERT_EQ(err, PRESSURE_ERR_FULL, "REJECT", "Should reject when full");

    pressure_stats_t s;
    pressure_get_stats(&q, &s);
    ASSERT_EQ(s.rejected, 1, "REJECT", "Should track 1 rejection");

    TEST_PASS("Policy: REJECT works correctly");
}

/**
 * DROP_OLDEST policy test
 */
static void test_policy_drop_oldest(void)
{
    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 4;
    cfg.policy = POLICY_DROP_OLDEST;
    cfg.high_water = 3;
    cfg.low_water = 1;
    cfg.critical_water = 4;
    pressure_item_t buffer[4];

    pressure_init(&q, &cfg, buffer);

    /* Enqueue 1-6 */
    for (uint64_t i = 1; i <= 6; i++) {
        pressure_enqueue(&q, i * 10, i, NULL);
    }

    /* Should contain 30, 40, 50, 60 */
    pressure_item_t item;
    pressure_dequeue(&q, &item, NULL);
    ASSERT_EQ(item.payload, 30, "DROP_OLDEST", "First should be 30");

    pressure_stats_t s;
    pressure_get_stats(&q, &s);
    ASSERT_EQ(s.dropped_oldest, 2, "DROP_OLDEST", "Should drop 2 items");

    TEST_PASS("Policy: DROP_OLDEST works correctly");
}

/**
 * DROP_NEWEST policy test
 */
static void test_policy_drop_newest(void)
{
    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 4;
    cfg.policy = POLICY_DROP_NEWEST;
    cfg.high_water = 3;
    cfg.low_water = 1;
    cfg.critical_water = 4;
    pressure_item_t buffer[4];

    pressure_init(&q, &cfg, buffer);

    /* Enqueue 1-6 */
    for (uint64_t i = 1; i <= 6; i++) {
        pressure_result_t r;
        pressure_enqueue(&q, i * 10, i, &r);
        if (i > 4) {
            ASSERT_TRUE(r.was_dropped, "DROP_NEWEST", "Should mark as dropped");
        }
    }

    /* Should contain 10, 20, 30, 40 */
    pressure_item_t item;
    pressure_dequeue(&q, &item, NULL);
    ASSERT_EQ(item.payload, 10, "DROP_NEWEST", "First should be 10");

    pressure_stats_t s;
    pressure_get_stats(&q, &s);
    ASSERT_EQ(s.dropped_newest, 2, "DROP_NEWEST", "Should drop 2 items");

    TEST_PASS("Policy: DROP_NEWEST works correctly");
}

/*===========================================================================
 * EDGE CASE TESTS
 *===========================================================================*/

/**
 * Empty queue dequeue
 */
static void test_edge_empty_dequeue(void)
{
    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 8;
    cfg.high_water = 6;
    cfg.low_water = 2;
    cfg.critical_water = 7;
    pressure_item_t buffer[8];

    pressure_init(&q, &cfg, buffer);

    pressure_item_t item;
    int err = pressure_dequeue(&q, &item, NULL);
    ASSERT_EQ(err, PRESSURE_ERR_EMPTY, "Edge", "Should return ERR_EMPTY");

    TEST_PASS("Edge: Empty dequeue returns ERR_EMPTY");
}

/**
 * Config validation
 */
static void test_edge_config_validation(void)
{
    pressure_queue_t q;
    pressure_item_t buffer[16];
    pressure_config_t cfg;
    int err;

    /* Zero capacity */
    cfg = pressure_default_config();
    cfg.capacity = 0;
    err = pressure_init(&q, &cfg, buffer);
    ASSERT_EQ(err, PRESSURE_ERR_CONFIG, "Edge", "Should reject capacity=0");

    /* Capacity too large */
    cfg = pressure_default_config();
    cfg.capacity = PRESSURE_MAX_CAPACITY + 1;
    err = pressure_init(&q, &cfg, buffer);
    ASSERT_EQ(err, PRESSURE_ERR_CONFIG, "Edge", "Should reject oversized capacity");

    /* Invalid water marks */
    cfg = pressure_default_config();
    cfg.low_water = cfg.high_water;  /* low must be < high */
    err = pressure_init(&q, &cfg, buffer);
    ASSERT_EQ(err, PRESSURE_ERR_CONFIG, "Edge", "Should reject low_water >= high_water");

    /* NULL pointers */
    cfg = pressure_default_config();
    err = pressure_init(NULL, &cfg, buffer);
    ASSERT_EQ(err, PRESSURE_ERR_NULL, "Edge", "Should reject NULL queue");

    err = pressure_init(&q, NULL, buffer);
    ASSERT_EQ(err, PRESSURE_ERR_NULL, "Edge", "Should reject NULL config");

    err = pressure_init(&q, &cfg, NULL);
    ASSERT_EQ(err, PRESSURE_ERR_NULL, "Edge", "Should reject NULL buffer");

    TEST_PASS("Edge: Config validation works");
}

/**
 * Reset clears state
 */
static void test_edge_reset(void)
{
    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 8;
    cfg.high_water = 6;
    cfg.low_water = 2;
    cfg.critical_water = 7;
    pressure_item_t buffer[8];

    pressure_init(&q, &cfg, buffer);

    /* Fill queue */
    for (uint64_t i = 0; i < 8; i++) {
        pressure_enqueue(&q, i, i, NULL);
    }
    ASSERT_TRUE(q.count > 0, "Edge", "Should have items");

    /* Reset */
    pressure_reset(&q);

    ASSERT_EQ(q.count, 0, "Edge", "Count should be 0 after reset");
    ASSERT_EQ(q.head, 0, "Edge", "Head should be 0 after reset");
    ASSERT_EQ(q.tail, 0, "Edge", "Tail should be 0 after reset");
    ASSERT_EQ(q.state, PRESSURE_LOW, "Edge", "State should be LOW after reset");
    ASSERT_EQ(q.stats.enqueued, 0, "Edge", "Stats should be cleared");

    TEST_PASS("Edge: Reset clears state correctly");
}

/*===========================================================================
 * FUZZ TEST
 *===========================================================================*/

static void test_fuzz_random_ops(void)
{
    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 32;
    cfg.policy = POLICY_DROP_OLDEST;
    cfg.high_water = 24;
    cfg.low_water = 8;
    cfg.critical_water = 30;
    pressure_item_t buffer[32];

    pressure_init(&q, &cfg, buffer);

    srand((unsigned int)time(NULL));

    for (int i = 0; i < 100000; i++) {
        int op = rand() % 10;

        if (op < 7) {  /* 70% enqueue */
            pressure_enqueue(&q, rand(), i, NULL);
        } else {  /* 30% dequeue */
            pressure_item_t item;
            pressure_dequeue(&q, &item, NULL);
        }

        /* Check invariants */
        if (q.count > q.cfg.capacity) {
            TEST_FAIL("Fuzz", "count > capacity");
            return;
        }
        if (q.head >= q.cfg.capacity || q.tail >= q.cfg.capacity) {
            TEST_FAIL("Fuzz", "head or tail out of bounds");
            return;
        }
    }

    /* Verify accounting */
    pressure_stats_t s;
    pressure_get_stats(&q, &s);
    uint64_t total = s.dequeued + q.count + s.dropped_oldest + s.dropped_newest + s.rejected;
    if (s.enqueued != total) {
        TEST_FAIL("Fuzz", "Accounting mismatch after fuzz");
        return;
    }

    TEST_PASS("Fuzz: 100000 random ops, invariants held");
}

/*===========================================================================
 * MAIN
 *===========================================================================*/

int main(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║           PRESSURE Contract Test Suite                         ║\n");
    printf("║           Module 6: Bounded Queue with Backpressure            ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    printf("Contract Tests:\n");
    test_contract1_bounded_memory();
    test_contract1b_bounded_reject();
    test_contract2_accounting();
    test_contract3_fifo();
    test_contract3b_fifo_wraparound();
    test_contract4_pressure_signal();
    printf("\n");

    printf("Invariant Tests:\n");
    test_inv1_count_bound();
    test_inv2_index_bounds();
    test_inv3_sequence_monotonic();
    printf("\n");

    printf("Policy Tests:\n");
    test_policy_reject();
    test_policy_drop_oldest();
    test_policy_drop_newest();
    printf("\n");

    printf("Edge Case Tests:\n");
    test_edge_empty_dequeue();
    test_edge_config_validation();
    test_edge_reset();
    printf("\n");

    printf("Fuzz Tests:\n");
    test_fuzz_random_ops();
    printf("\n");

    printf("══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("══════════════════════════════════════════════════════════════════\n");
    printf("\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
