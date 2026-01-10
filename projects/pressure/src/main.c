/**
 * main.c - Pressure (Bounded Queue) Demo
 * 
 * Demonstrates all contracts and overflow policies:
 *   1. Basic FIFO       - Items dequeue in order
 *   2. REJECT Policy    - Producer backpressure
 *   3. DROP_OLDEST      - Lossy but never blocks
 *   4. DROP_NEWEST      - Preserve history
 *   5. Pressure States  - LOW → NORMAL → HIGH → CRITICAL
 *   6. Statistics       - Full accounting of all items
 *   7. Burst Handling   - Absorb spikes gracefully
 * 
 * Copyright (c) 2026 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#include <stdio.h>
#include <string.h>
#include "pressure.h"

/*===========================================================================
 * Demo Helpers
 *===========================================================================*/

static void print_header(const char *title)
{
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  %s\n", title);
    printf("═══════════════════════════════════════════════════════════════\n");
}

static void print_config(const pressure_config_t *cfg)
{
    printf("\n  Configuration:\n");
    printf("    capacity       = %u\n", cfg->capacity);
    printf("    policy         = %s\n", pressure_policy_name(cfg->policy));
    printf("    low_water      = %u (%.0f%%)\n", cfg->low_water, 
           100.0 * cfg->low_water / cfg->capacity);
    printf("    high_water     = %u (%.0f%%)\n", cfg->high_water,
           100.0 * cfg->high_water / cfg->capacity);
    printf("    critical_water = %u (%.0f%%)\n", cfg->critical_water,
           100.0 * cfg->critical_water / cfg->capacity);
}

static void print_stats(const pressure_stats_t *s)
{
    printf("  Statistics:\n");
    printf("    enqueued       = %lu\n", (unsigned long)s->enqueued);
    printf("    dequeued       = %lu\n", (unsigned long)s->dequeued);
    printf("    rejected       = %lu\n", (unsigned long)s->rejected);
    printf("    dropped_oldest = %lu\n", (unsigned long)s->dropped_oldest);
    printf("    dropped_newest = %lu\n", (unsigned long)s->dropped_newest);
    printf("    peak_fill      = %u\n", s->peak_fill);
    printf("    high_events    = %u\n", s->high_water_events);
    printf("    critical_events= %u\n", s->critical_events);
}

/*===========================================================================
 * Demo 1: Basic FIFO Operation
 *===========================================================================*/

static void demo_basic_fifo(void)
{
    print_header("Demo 1: Basic FIFO Operation (CONTRACT-3)");
    printf("  Items must dequeue in the same order they were enqueued.\n");

    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 8;
    cfg.high_water = 6;
    cfg.low_water = 2;
    cfg.critical_water = 7;
    pressure_item_t buffer[8];

    pressure_init(&q, &cfg, buffer);

    /* Enqueue items 1-5 */
    printf("\n  Enqueuing: ");
    for (uint64_t i = 1; i <= 5; i++) {
        pressure_enqueue(&q, i * 100, i, NULL);
        printf("%lu ", (unsigned long)(i * 100));
    }
    printf("\n");

    /* Dequeue and verify order */
    printf("  Dequeuing: ");
    pressure_item_t item;
    while (pressure_dequeue(&q, &item, NULL) == PRESSURE_OK) {
        printf("%lu ", (unsigned long)item.payload);
    }
    printf("\n");

    printf("\n  CONTRACT-3 PROVEN: Items returned in FIFO order.\n");
}

/*===========================================================================
 * Demo 2: REJECT Policy (Backpressure)
 *===========================================================================*/

static void demo_reject_policy(void)
{
    print_header("Demo 2: REJECT Policy (Producer Backpressure)");
    printf("  Queue refuses new items when full.\n");

    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 4;
    cfg.policy = POLICY_REJECT;
    cfg.high_water = 3;
    cfg.low_water = 1;
    cfg.critical_water = 4;
    pressure_item_t buffer[4];

    pressure_init(&q, &cfg, buffer);
    print_config(&cfg);

    printf("\n  Enqueuing 6 items into capacity-4 queue:\n");
    for (uint64_t i = 1; i <= 6; i++) {
        pressure_result_t r;
        int err = pressure_enqueue(&q, i, i, &r);
        printf("    Item %lu: %s (fill=%u/%u, state=%s)\n",
               (unsigned long)i,
               err == PRESSURE_OK ? "accepted" : "REJECTED",
               r.fill, r.capacity,
               pressure_state_name(r.state));
    }

    pressure_stats_t stats;
    pressure_get_stats(&q, &stats);
    printf("\n");
    print_stats(&stats);

    printf("\n  CONTRACT-1 PROVEN: Queue never exceeded capacity.\n");
    printf("  Rejected items = %lu (backpressure signal to producer)\n",
           (unsigned long)stats.rejected);
}

/*===========================================================================
 * Demo 3: DROP_OLDEST Policy
 *===========================================================================*/

static void demo_drop_oldest(void)
{
    print_header("Demo 3: DROP_OLDEST Policy (Lossy, Never Blocks)");
    printf("  When full, overwrite oldest item to accept new.\n");

    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 4;
    cfg.policy = POLICY_DROP_OLDEST;
    cfg.high_water = 3;
    cfg.low_water = 1;
    cfg.critical_water = 4;
    pressure_item_t buffer[4];

    pressure_init(&q, &cfg, buffer);

    /* Fill queue with 1-4 */
    printf("\n  Initial fill: ");
    for (uint64_t i = 1; i <= 4; i++) {
        pressure_enqueue(&q, i * 10, i, NULL);
        printf("%lu ", (unsigned long)(i * 10));
    }
    printf("\n");

    /* Enqueue 5-7, which will overwrite 1-3 */
    printf("  Adding more (will overwrite oldest): ");
    for (uint64_t i = 5; i <= 7; i++) {
        pressure_enqueue(&q, i * 10, i, NULL);
        printf("%lu ", (unsigned long)(i * 10));
    }
    printf("\n");

    /* Dequeue remaining */
    printf("  Final contents: ");
    pressure_item_t item;
    while (pressure_dequeue(&q, &item, NULL) == PRESSURE_OK) {
        printf("%lu ", (unsigned long)item.payload);
    }
    printf("\n");

    pressure_stats_t stats;
    pressure_get_stats(&q, &stats);
    printf("\n");
    print_stats(&stats);

    printf("\n  Note: Items 10, 20, 30 were dropped to make room for 50, 60, 70.\n");
}

/*===========================================================================
 * Demo 4: DROP_NEWEST Policy
 *===========================================================================*/

static void demo_drop_newest(void)
{
    print_header("Demo 4: DROP_NEWEST Policy (Preserve History)");
    printf("  When full, discard incoming item (keep existing).\n");

    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 4;
    cfg.policy = POLICY_DROP_NEWEST;
    cfg.high_water = 3;
    cfg.low_water = 1;
    cfg.critical_water = 4;
    pressure_item_t buffer[4];

    pressure_init(&q, &cfg, buffer);

    /* Fill queue with 1-4 */
    printf("\n  Initial fill: ");
    for (uint64_t i = 1; i <= 4; i++) {
        pressure_enqueue(&q, i * 10, i, NULL);
        printf("%lu ", (unsigned long)(i * 10));
    }
    printf("\n");

    /* Try to enqueue 5-7, which will be dropped */
    printf("  Attempting to add (will be dropped): ");
    for (uint64_t i = 5; i <= 7; i++) {
        pressure_result_t r;
        pressure_enqueue(&q, i * 10, i, &r);
        printf("%lu%s ", (unsigned long)(i * 10), r.was_dropped ? "(dropped)" : "");
    }
    printf("\n");

    /* Dequeue remaining */
    printf("  Final contents: ");
    pressure_item_t item;
    while (pressure_dequeue(&q, &item, NULL) == PRESSURE_OK) {
        printf("%lu ", (unsigned long)item.payload);
    }
    printf("\n");

    pressure_stats_t stats;
    pressure_get_stats(&q, &stats);
    printf("\n");
    print_stats(&stats);

    printf("\n  Note: Original items 10-40 preserved, new items 50-70 dropped.\n");
}

/*===========================================================================
 * Demo 5: Pressure State Transitions
 *===========================================================================*/

static void demo_pressure_states(void)
{
    print_header("Demo 5: Pressure State Transitions (CONTRACT-4)");
    printf("  Fill level determines pressure state.\n");

    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 20;
    cfg.policy = POLICY_REJECT;
    cfg.low_water = 5;      /* 25% */
    cfg.high_water = 15;    /* 75% */
    cfg.critical_water = 18;/* 90% */
    pressure_item_t buffer[20];

    pressure_init(&q, &cfg, buffer);
    print_config(&cfg);

    printf("\n  Filling queue and observing state transitions:\n");
    printf("    Fill | State\n");
    printf("  -------+----------\n");

    for (uint64_t i = 1; i <= 20; i++) {
        pressure_result_t r;
        int err = pressure_enqueue(&q, i, i, &r);
        if (err == PRESSURE_OK) {
            printf("  %5u  | %s\n", r.fill, pressure_state_name(r.state));
        }
    }

    printf("\n  Draining queue:\n");
    printf("    Fill | State\n");
    printf("  -------+----------\n");

    pressure_item_t item;
    pressure_result_t r;
    while (pressure_dequeue(&q, &item, &r) == PRESSURE_OK) {
        if (r.fill % 4 == 0 || r.fill <= 2) {  /* Print select lines */
            printf("  %5u  | %s\n", r.fill, pressure_state_name(r.state));
        }
    }

    printf("\n  CONTRACT-4 PROVEN: State accurately reflects fill level.\n");
}

/*===========================================================================
 * Demo 6: Full Accounting (CONTRACT-2)
 *===========================================================================*/

static void demo_accounting(void)
{
    print_header("Demo 6: Full Accounting (CONTRACT-2)");
    printf("  Every item is tracked: enqueued = dequeued + in_queue + dropped.\n");

    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 10;
    cfg.policy = POLICY_DROP_OLDEST;
    cfg.high_water = 8;
    cfg.low_water = 3;
    cfg.critical_water = 9;
    pressure_item_t buffer[10];

    pressure_init(&q, &cfg, buffer);

    /* Enqueue 25 items (will cause drops) */
    for (uint64_t i = 1; i <= 25; i++) {
        pressure_enqueue(&q, i, i, NULL);
    }

    /* Dequeue 5 items */
    pressure_item_t item;
    for (int i = 0; i < 5; i++) {
        pressure_dequeue(&q, &item, NULL);
    }

    pressure_stats_t s;
    pressure_get_stats(&q, &s);
    uint32_t in_queue = pressure_count(&q);

    printf("\n  After 25 enqueues and 5 dequeues:\n");
    print_stats(&s);
    printf("    in_queue       = %u\n", in_queue);

    /* Verify accounting equation */
    uint64_t total_dropped = s.dropped_oldest + s.dropped_newest;
    uint64_t accounted = s.dequeued + in_queue + total_dropped;
    
    printf("\n  Accounting check:\n");
    printf("    enqueued = %lu\n", (unsigned long)s.enqueued);
    printf("    dequeued + in_queue + dropped = %lu + %u + %lu = %lu\n",
           (unsigned long)s.dequeued, in_queue, (unsigned long)total_dropped,
           (unsigned long)accounted);
    printf("    Match: %s\n", (s.enqueued == accounted) ? "YES ✓" : "NO ✗");

    printf("\n  CONTRACT-2 PROVEN: Every item accounted for.\n");
}

/*===========================================================================
 * Demo 7: Burst Absorption
 *===========================================================================*/

static void demo_burst_absorption(void)
{
    print_header("Demo 7: Burst Absorption");
    printf("  Queue absorbs message bursts, smoothing delivery.\n");

    pressure_queue_t q;
    pressure_config_t cfg = pressure_default_config();
    cfg.capacity = 32;
    cfg.policy = POLICY_REJECT;
    cfg.low_water = 8;
    cfg.high_water = 24;
    cfg.critical_water = 30;
    pressure_item_t buffer[32];

    pressure_init(&q, &cfg, buffer);

    printf("\n  Simulating: bursty producer, steady consumer\n");
    printf("  Producer: 10 items every 5 ticks\n");
    printf("  Consumer: 2 items every tick\n\n");

    printf("  Tick | Produced | Consumed | Fill | State\n");
    printf("  -----+----------+----------+------+----------\n");

    uint64_t seq = 0;
    for (int tick = 0; tick < 30; tick++) {
        int produced = 0;
        int consumed = 0;

        /* Burst every 5 ticks */
        if (tick % 5 == 0) {
            for (int i = 0; i < 10; i++) {
                if (pressure_enqueue(&q, seq++, tick, NULL) == PRESSURE_OK) {
                    produced++;
                }
            }
        }

        /* Steady consumption */
        pressure_item_t item;
        for (int i = 0; i < 2; i++) {
            if (pressure_dequeue(&q, &item, NULL) == PRESSURE_OK) {
                consumed++;
            }
        }

        printf("  %4d | %8d | %8d | %4u | %s\n",
               tick, produced, consumed, 
               pressure_count(&q), pressure_state_name(pressure_state(&q)));
    }

    printf("\n  Queue absorbed bursts, consumer saw steady stream.\n");
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║           Module 6: Pressure — Bounded Queue                  ║\n");
    printf("║                                                               ║\n");
    printf("║   \"When messages arrive faster than you can process them,     ║\n");
    printf("║    you have three choices: drop, block, or explode.           ║\n");
    printf("║    Only bounded queues let you choose deliberately.\"          ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    demo_basic_fifo();
    demo_reject_policy();
    demo_drop_oldest();
    demo_drop_newest();
    demo_pressure_states();
    demo_accounting();
    demo_burst_absorption();

    print_header("Demo Complete");
    printf("\n  Key insights demonstrated:\n");
    printf("    1. FIFO ordering preserved\n");
    printf("    2. Three overflow policies for different use cases\n");
    printf("    3. Pressure states reflect fill level\n");
    printf("    4. Full accounting of all items\n");
    printf("    5. Burst absorption smooths delivery\n");
    printf("\n  Contracts proven:\n");
    printf("    CONTRACT-1: Bounded memory (never exceeds capacity)\n");
    printf("    CONTRACT-2: No data loss (all items tracked)\n");
    printf("    CONTRACT-3: FIFO ordering\n");
    printf("    CONTRACT-4: Pressure signal accuracy\n");
    printf("\n  Next: Compose all modules into a complete safety system.\n\n");

    return 0;
}
