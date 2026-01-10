/**
 * pressure.h - Bounded Queue with Backpressure
 * 
 * A closed, total, deterministic state machine for managing
 * message flow under load using bounded ring buffers.
 * 
 * Module 1 proved existence in time.
 * Module 2 proved normality in value.
 * Module 3 proved health over time.
 * Module 4 proved velocity toward failure.
 * Module 5 proved truth from many liars.
 * Module 6 proves graceful degradation under pressure.
 * 
 * THE CORE INSIGHT:
 *   "When messages arrive faster than you can process them,
 *    you have three choices: drop, block, or explode.
 *    Only bounded queues let you choose deliberately."
 * 
 * CONTRACTS:
 *   1. BOUNDED MEMORY:    Queue never exceeds configured capacity
 *   2. NO DATA LOSS:      Every item is either queued, rejected, or dropped (tracked)
 *   3. FIFO ORDERING:     Items dequeue in insertion order
 *   4. PRESSURE SIGNAL:   Fill level accurately reflects queue state
 * 
 * OVERFLOW POLICIES:
 *   - REJECT:  Refuse new items when full (producer backpressure)
 *   - DROP_OLDEST: Overwrite oldest item (lossy but never blocks)
 *   - DROP_NEWEST: Discard incoming item (preserve history)
 * 
 * See: lessons/ for proofs and data dictionary
 * 
 * Copyright (c) 2026 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#ifndef PRESSURE_H
#define PRESSURE_H

#include <stdint.h>
#include <stddef.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define PRESSURE_MAX_CAPACITY 4096  /* Maximum supported queue size */

/*===========================================================================
 * Error Codes
 *===========================================================================*/

typedef enum {
    PRESSURE_OK           =  0,  /* Success */
    PRESSURE_ERR_NULL     = -1,  /* NULL pointer passed */
    PRESSURE_ERR_CONFIG   = -2,  /* Invalid configuration */
    PRESSURE_ERR_FULL     = -3,  /* Queue full (REJECT policy) */
    PRESSURE_ERR_EMPTY    = -4,  /* Queue empty on dequeue */
    PRESSURE_ERR_FAULT    = -5,  /* Module in fault state */
    PRESSURE_ERR_REENTRY  = -6,  /* Reentrancy violation */
    PRESSURE_ERR_OVERFLOW = -7   /* Counter overflow */
} pressure_error_t;

/*===========================================================================
 * Overflow Policies
 *===========================================================================*/

/**
 * What to do when queue is full and a new item arrives.
 */
typedef enum {
    POLICY_REJECT      = 0,  /* Refuse new item, return ERR_FULL */
    POLICY_DROP_OLDEST = 1,  /* Overwrite oldest, continue accepting */
    POLICY_DROP_NEWEST = 2   /* Discard new item silently (but track) */
} overflow_policy_t;

/*===========================================================================
 * Pressure States
 *===========================================================================*/

/**
 * Queue pressure states based on fill level.
 * 
 * Thresholds (configurable):
 *   LOW:      fill < 25%
 *   NORMAL:   25% <= fill < 75%
 *   HIGH:     75% <= fill < 90%
 *   CRITICAL: fill >= 90%
 */
typedef enum {
    PRESSURE_LOW      = 0,  /* Plenty of capacity */
    PRESSURE_NORMAL   = 1,  /* Operating normally */
    PRESSURE_HIGH     = 2,  /* Consider slowing producer */
    PRESSURE_CRITICAL = 3,  /* Near capacity, backpressure needed */
    PRESSURE_FAULT    = 4   /* Internal fault */
} pressure_state_t;

/*===========================================================================
 * Configuration
 *===========================================================================*/

/**
 * Configuration parameters (immutable after init).
 * 
 * CONSTRAINTS:
 *   C1: capacity > 0 && capacity <= PRESSURE_MAX_CAPACITY
 *   C2: policy ∈ {REJECT, DROP_OLDEST, DROP_NEWEST}
 *   C3: high_water <= capacity
 *   C4: low_water < high_water
 */
typedef struct {
    uint32_t         capacity;      /* Maximum items in queue */
    overflow_policy_t policy;       /* What to do when full */
    uint32_t         high_water;    /* Threshold for HIGH state (default: 75%) */
    uint32_t         low_water;     /* Threshold for return to NORMAL (default: 25%) */
    uint32_t         critical_water;/* Threshold for CRITICAL (default: 90%) */
} pressure_config_t;

/**
 * Default configuration.
 * 
 * capacity = 64
 * policy = POLICY_REJECT
 * high_water = 48 (75%)
 * low_water = 16 (25%)
 * critical_water = 58 (90%)
 */
#define PRESSURE_DEFAULT_CAPACITY 64

static inline pressure_config_t pressure_default_config(void) {
    pressure_config_t cfg = {
        .capacity = PRESSURE_DEFAULT_CAPACITY,
        .policy = POLICY_REJECT,
        .high_water = 48,
        .low_water = 16,
        .critical_water = 58
    };
    return cfg;
}

/*===========================================================================
 * Queue Item
 *===========================================================================*/

/**
 * A single queue item.
 * 
 * Generic payload with timestamp for age tracking.
 */
typedef struct {
    uint64_t timestamp;   /* When item was enqueued */
    uint64_t payload;     /* Generic 64-bit payload (or pointer) */
    uint32_t sequence;    /* Monotonic sequence number */
} pressure_item_t;

/*===========================================================================
 * Statistics
 *===========================================================================*/

/**
 * Queue statistics for monitoring and diagnostics.
 */
typedef struct {
    uint64_t enqueued;        /* Total items successfully enqueued */
    uint64_t dequeued;        /* Total items successfully dequeued */
    uint64_t rejected;        /* Items rejected (REJECT policy) */
    uint64_t dropped_oldest;  /* Items dropped (DROP_OLDEST policy) */
    uint64_t dropped_newest;  /* Items dropped (DROP_NEWEST policy) */
    uint32_t high_water_events;   /* Times HIGH state entered */
    uint32_t critical_events;     /* Times CRITICAL state entered */
    uint32_t peak_fill;       /* Maximum fill level observed */
} pressure_stats_t;

/*===========================================================================
 * Result Structure
 *===========================================================================*/

/**
 * Result of enqueue/dequeue operations.
 */
typedef struct {
    pressure_state_t state;       /* Current pressure state */
    uint32_t         fill;        /* Current item count */
    uint32_t         capacity;    /* Queue capacity */
    double           fill_ratio;  /* fill / capacity (0.0 to 1.0) */
    uint8_t          was_dropped; /* Item was dropped (for enqueue) */
    uint8_t          valid;       /* Operation succeeded */
} pressure_result_t;

/*===========================================================================
 * FSM Structure
 *===========================================================================*/

/**
 * Pressure queue finite state machine.
 * 
 * INVARIANTS:
 *   INV-1: count <= capacity (always)
 *   INV-2: head, tail < capacity (always)
 *   INV-3: (count == 0) ↔ (head == tail && !wrapped)
 *   INV-4: stats.enqueued == stats.dequeued + count + stats.dropped_*
 *   INV-5: state reflects fill level accurately
 * 
 * RING BUFFER MECHANICS:
 *   - head: next position to read from
 *   - tail: next position to write to
 *   - count: number of items currently in queue
 */
typedef struct {
    /* Configuration (immutable after init) */
    pressure_config_t cfg;
    
    /* Ring buffer storage (allocated separately) */
    pressure_item_t *buffer;
    
    /* Ring buffer indices */
    uint32_t head;      /* Read position */
    uint32_t tail;      /* Write position */
    uint32_t count;     /* Current item count */
    
    /* Sequence counter for ordering */
    uint32_t next_seq;
    
    /* State */
    pressure_state_t state;
    
    /* Statistics */
    pressure_stats_t stats;
    
    /* Fault flags */
    uint8_t fault_reentry;
    uint8_t fault_overflow;
    
    /* Atomicity guard */
    uint8_t in_operation;
    
    /* Initialisation flag */
    uint8_t initialized;
} pressure_queue_t;

/*===========================================================================
 * Public API
 *===========================================================================*/

/**
 * Initialise the pressure queue.
 * 
 * @param q       Pointer to queue structure
 * @param cfg     Configuration parameters
 * @param buffer  Pre-allocated buffer of size cfg->capacity
 * @return        PRESSURE_OK on success, negative error code on failure
 *
 * PRE:  q != NULL, cfg != NULL, buffer != NULL
 * PRE:  cfg->capacity > 0 && cfg->capacity <= PRESSURE_MAX_CAPACITY
 * POST: q->state == PRESSURE_LOW
 * POST: q->count == 0
 * 
 * NOTE: Caller must allocate buffer. This allows static allocation:
 *       pressure_item_t buffer[64];
 *       pressure_init(&q, &cfg, buffer);
 */
int pressure_init(pressure_queue_t *q, 
                  const pressure_config_t *cfg,
                  pressure_item_t *buffer);

/**
 * Enqueue an item.
 * 
 * @param q         Pointer to initialised queue
 * @param payload   64-bit payload value
 * @param timestamp Current timestamp
 * @param result    Pointer to result structure (optional, may be NULL)
 * @return          PRESSURE_OK on success, negative error code on failure
 *
 * BEHAVIOUR BY POLICY:
 *   REJECT:      Returns ERR_FULL if queue is full
 *   DROP_OLDEST: Overwrites oldest item, always succeeds
 *   DROP_NEWEST: Discards new item if full, returns OK but result->was_dropped=1
 *
 * PRE:  q != NULL, q->initialized
 * POST: count <= capacity (CONTRACT-1)
 * POST: item tracked in stats (CONTRACT-2)
 */
int pressure_enqueue(pressure_queue_t *q,
                     uint64_t payload,
                     uint64_t timestamp,
                     pressure_result_t *result);

/**
 * Dequeue an item.
 * 
 * @param q      Pointer to initialised queue
 * @param item   Pointer to receive dequeued item
 * @param result Pointer to result structure (optional, may be NULL)
 * @return       PRESSURE_OK on success, ERR_EMPTY if queue empty
 *
 * PRE:  q != NULL, item != NULL
 * POST: Items returned in FIFO order (CONTRACT-3)
 */
int pressure_dequeue(pressure_queue_t *q,
                     pressure_item_t *item,
                     pressure_result_t *result);

/**
 * Peek at the oldest item without removing it.
 * 
 * @param q    Pointer to initialised queue
 * @param item Pointer to receive item copy
 * @return     PRESSURE_OK on success, ERR_EMPTY if queue empty
 */
int pressure_peek(const pressure_queue_t *q, pressure_item_t *item);

/**
 * Get current queue statistics.
 * 
 * @param q     Pointer to queue
 * @param stats Pointer to receive statistics
 * @return      PRESSURE_OK on success
 */
int pressure_get_stats(const pressure_queue_t *q, pressure_stats_t *stats);

/**
 * Reset queue to empty state.
 * Preserves configuration, clears state, stats, and faults.
 *
 * @param q Pointer to queue
 */
void pressure_reset(pressure_queue_t *q);

/**
 * Clear statistics only (keep queue contents).
 * 
 * @param q Pointer to queue
 */
void pressure_clear_stats(pressure_queue_t *q);

/*===========================================================================
 * Query Functions (Inline)
 *===========================================================================*/

/**
 * Get current pressure state.
 */
static inline pressure_state_t pressure_state(const pressure_queue_t *q) {
    return q ? q->state : PRESSURE_FAULT;
}

/**
 * Get current fill count.
 */
static inline uint32_t pressure_count(const pressure_queue_t *q) {
    return q ? q->count : 0;
}

/**
 * Get queue capacity.
 */
static inline uint32_t pressure_capacity(const pressure_queue_t *q) {
    return q ? q->cfg.capacity : 0;
}

/**
 * Check if queue is empty.
 */
static inline uint8_t pressure_is_empty(const pressure_queue_t *q) {
    return q ? (q->count == 0) : 1;
}

/**
 * Check if queue is full.
 */
static inline uint8_t pressure_is_full(const pressure_queue_t *q) {
    return q ? (q->count >= q->cfg.capacity) : 0;
}

/**
 * Get fill ratio (0.0 to 1.0).
 */
static inline double pressure_fill_ratio(const pressure_queue_t *q) {
    if (!q || q->cfg.capacity == 0) return 0.0;
    return (double)q->count / (double)q->cfg.capacity;
}

/**
 * Check if any fault is active.
 */
static inline uint8_t pressure_faulted(const pressure_queue_t *q) {
    return q ? (q->fault_reentry || q->fault_overflow) : 1;
}

/**
 * Convert state to string.
 */
static inline const char* pressure_state_name(pressure_state_t st) {
    switch (st) {
        case PRESSURE_LOW:      return "LOW";
        case PRESSURE_NORMAL:   return "NORMAL";
        case PRESSURE_HIGH:     return "HIGH";
        case PRESSURE_CRITICAL: return "CRITICAL";
        case PRESSURE_FAULT:    return "FAULT";
        default:                return "INVALID";
    }
}

/**
 * Convert policy to string.
 */
static inline const char* pressure_policy_name(overflow_policy_t p) {
    switch (p) {
        case POLICY_REJECT:      return "REJECT";
        case POLICY_DROP_OLDEST: return "DROP_OLDEST";
        case POLICY_DROP_NEWEST: return "DROP_NEWEST";
        default:                 return "INVALID";
    }
}

/**
 * Convert error code to string.
 */
static inline const char* pressure_error_name(pressure_error_t err) {
    switch (err) {
        case PRESSURE_OK:           return "OK";
        case PRESSURE_ERR_NULL:     return "ERR_NULL";
        case PRESSURE_ERR_CONFIG:   return "ERR_CONFIG";
        case PRESSURE_ERR_FULL:     return "ERR_FULL";
        case PRESSURE_ERR_EMPTY:    return "ERR_EMPTY";
        case PRESSURE_ERR_FAULT:    return "ERR_FAULT";
        case PRESSURE_ERR_REENTRY:  return "ERR_REENTRY";
        case PRESSURE_ERR_OVERFLOW: return "ERR_OVERFLOW";
        default:                    return "UNKNOWN";
    }
}

#endif /* PRESSURE_H */
