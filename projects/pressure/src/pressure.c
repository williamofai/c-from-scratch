/**
 * pressure.c - Bounded Queue with Backpressure
 * 
 * Implementation of the pressure queue finite state machine.
 * 
 * RING BUFFER IMPLEMENTATION:
 *   - Fixed-size circular buffer
 *   - head: index of oldest item (next to dequeue)
 *   - tail: index of next write position
 *   - count: number of items currently stored
 * 
 * The count variable disambiguates full vs empty:
 *   - Empty: count == 0
 *   - Full:  count == capacity
 * 
 * Copyright (c) 2026 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#include "pressure.h"
#include <string.h>

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * Update pressure state based on current fill level.
 */
static void update_state(pressure_queue_t *q)
{
    if (q->count >= q->cfg.critical_water) {
        if (q->state != PRESSURE_CRITICAL) {
            q->stats.critical_events++;
        }
        q->state = PRESSURE_CRITICAL;
    }
    else if (q->count >= q->cfg.high_water) {
        if (q->state != PRESSURE_HIGH && q->state != PRESSURE_CRITICAL) {
            q->stats.high_water_events++;
        }
        q->state = PRESSURE_HIGH;
    }
    else if (q->count >= q->cfg.low_water) {
        q->state = PRESSURE_NORMAL;
    }
    else {
        q->state = PRESSURE_LOW;
    }
    
    /* Track peak fill */
    if (q->count > q->stats.peak_fill) {
        q->stats.peak_fill = q->count;
    }
}

/**
 * Fill result structure.
 */
static void fill_result(const pressure_queue_t *q, pressure_result_t *result, uint8_t dropped)
{
    if (result == NULL) return;
    
    result->state = q->state;
    result->fill = q->count;
    result->capacity = q->cfg.capacity;
    result->fill_ratio = (q->cfg.capacity > 0) 
                         ? (double)q->count / (double)q->cfg.capacity 
                         : 0.0;
    result->was_dropped = dropped;
    result->valid = 1;
}

/*===========================================================================
 * Public API
 *===========================================================================*/

/**
 * Initialise the queue.
 */
int pressure_init(pressure_queue_t *q, 
                  const pressure_config_t *cfg,
                  pressure_item_t *buffer)
{
    /* Null checks */
    if (q == NULL || cfg == NULL || buffer == NULL) {
        return PRESSURE_ERR_NULL;
    }

    /* C1: Valid capacity */
    if (cfg->capacity == 0 || cfg->capacity > PRESSURE_MAX_CAPACITY) {
        return PRESSURE_ERR_CONFIG;
    }

    /* C2: Valid policy */
    if (cfg->policy > POLICY_DROP_NEWEST) {
        return PRESSURE_ERR_CONFIG;
    }

    /* C3: high_water <= capacity */
    if (cfg->high_water > cfg->capacity) {
        return PRESSURE_ERR_CONFIG;
    }

    /* C4: low_water < high_water */
    if (cfg->low_water >= cfg->high_water) {
        return PRESSURE_ERR_CONFIG;
    }

    /* C5: critical_water reasonable */
    if (cfg->critical_water > cfg->capacity || cfg->critical_water <= cfg->high_water) {
        return PRESSURE_ERR_CONFIG;
    }

    /* Clear structure */
    memset(q, 0, sizeof(*q));

    /* Store configuration */
    q->cfg = *cfg;
    q->buffer = buffer;

    /* Initialise ring buffer */
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->next_seq = 1;

    /* Initial state */
    q->state = PRESSURE_LOW;
    q->initialized = 1;

    return PRESSURE_OK;
}

/**
 * Enqueue an item.
 */
int pressure_enqueue(pressure_queue_t *q,
                     uint64_t payload,
                     uint64_t timestamp,
                     pressure_result_t *result)
{
    /* Initialise result to safe defaults */
    if (result != NULL) {
        result->state = PRESSURE_FAULT;
        result->fill = 0;
        result->capacity = 0;
        result->fill_ratio = 0.0;
        result->was_dropped = 0;
        result->valid = 0;
    }

    /* Null check */
    if (q == NULL) {
        return PRESSURE_ERR_NULL;
    }

    /* Check initialisation */
    if (!q->initialized) {
        return PRESSURE_ERR_FAULT;
    }

    /* Reentrancy guard */
    if (q->in_operation) {
        q->fault_reentry = 1;
        q->state = PRESSURE_FAULT;
        return PRESSURE_ERR_REENTRY;
    }
    q->in_operation = 1;

    /* Check for existing fault */
    if (pressure_faulted(q)) {
        q->in_operation = 0;
        return PRESSURE_ERR_FAULT;
    }

    /* Check for full queue */
    if (q->count >= q->cfg.capacity) {
        /* Queue is full - apply overflow policy */
        switch (q->cfg.policy) {
            case POLICY_REJECT:
                q->stats.rejected++;
                fill_result(q, result, 0);
                q->in_operation = 0;
                return PRESSURE_ERR_FULL;

            case POLICY_DROP_OLDEST:
                /* Overwrite oldest: advance head */
                q->head = (q->head + 1) % q->cfg.capacity;
                q->count--;  /* Will be incremented below */
                q->stats.dropped_oldest++;
                break;

            case POLICY_DROP_NEWEST:
                /* Discard new item */
                q->stats.dropped_newest++;
                fill_result(q, result, 1);
                q->in_operation = 0;
                return PRESSURE_OK;  /* Success, but item was dropped */
        }
    }

    /* Write item to tail position */
    q->buffer[q->tail].payload = payload;
    q->buffer[q->tail].timestamp = timestamp;
    q->buffer[q->tail].sequence = q->next_seq++;

    /* Advance tail */
    q->tail = (q->tail + 1) % q->cfg.capacity;
    q->count++;

    /* Update statistics */
    q->stats.enqueued++;

    /* Update state */
    update_state(q);

    /* Fill result */
    fill_result(q, result, 0);

    q->in_operation = 0;
    return PRESSURE_OK;
}

/**
 * Dequeue an item.
 */
int pressure_dequeue(pressure_queue_t *q,
                     pressure_item_t *item,
                     pressure_result_t *result)
{
    /* Initialise result to safe defaults */
    if (result != NULL) {
        result->state = PRESSURE_FAULT;
        result->fill = 0;
        result->capacity = 0;
        result->fill_ratio = 0.0;
        result->was_dropped = 0;
        result->valid = 0;
    }

    /* Null checks */
    if (q == NULL || item == NULL) {
        return PRESSURE_ERR_NULL;
    }

    /* Check initialisation */
    if (!q->initialized) {
        return PRESSURE_ERR_FAULT;
    }

    /* Reentrancy guard */
    if (q->in_operation) {
        q->fault_reentry = 1;
        q->state = PRESSURE_FAULT;
        return PRESSURE_ERR_REENTRY;
    }
    q->in_operation = 1;

    /* Check for existing fault */
    if (pressure_faulted(q)) {
        q->in_operation = 0;
        return PRESSURE_ERR_FAULT;
    }

    /* Check for empty queue */
    if (q->count == 0) {
        fill_result(q, result, 0);
        q->in_operation = 0;
        return PRESSURE_ERR_EMPTY;
    }

    /* Read item from head position */
    *item = q->buffer[q->head];

    /* Advance head */
    q->head = (q->head + 1) % q->cfg.capacity;
    q->count--;

    /* Update statistics */
    q->stats.dequeued++;

    /* Update state */
    update_state(q);

    /* Fill result */
    fill_result(q, result, 0);

    q->in_operation = 0;
    return PRESSURE_OK;
}

/**
 * Peek at oldest item without removing.
 */
int pressure_peek(const pressure_queue_t *q, pressure_item_t *item)
{
    if (q == NULL || item == NULL) {
        return PRESSURE_ERR_NULL;
    }

    if (!q->initialized) {
        return PRESSURE_ERR_FAULT;
    }

    if (q->count == 0) {
        return PRESSURE_ERR_EMPTY;
    }

    *item = q->buffer[q->head];
    return PRESSURE_OK;
}

/**
 * Get statistics.
 */
int pressure_get_stats(const pressure_queue_t *q, pressure_stats_t *stats)
{
    if (q == NULL || stats == NULL) {
        return PRESSURE_ERR_NULL;
    }

    *stats = q->stats;
    return PRESSURE_OK;
}

/**
 * Reset to empty state.
 */
void pressure_reset(pressure_queue_t *q)
{
    if (q == NULL) return;

    /* Preserve config and buffer pointer */
    pressure_config_t cfg = q->cfg;
    pressure_item_t *buffer = q->buffer;

    /* Clear structure */
    memset(q, 0, sizeof(*q));

    /* Restore config and buffer */
    q->cfg = cfg;
    q->buffer = buffer;

    /* Reset ring buffer */
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->next_seq = 1;

    /* Initial state */
    q->state = PRESSURE_LOW;
    q->initialized = 1;
}

/**
 * Clear statistics only.
 */
void pressure_clear_stats(pressure_queue_t *q)
{
    if (q == NULL) return;
    memset(&q->stats, 0, sizeof(q->stats));
}
