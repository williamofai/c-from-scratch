# c-from-scratch — Module 6: Pressure

## Lesson 3: Structs & State Encoding

---

## The Queue Structure

```c
typedef struct {
    pressure_config_t cfg;      /* Immutable config */
    pressure_item_t *buffer;    /* Ring buffer storage */
    uint32_t head;              /* Read position */
    uint32_t tail;              /* Write position */
    uint32_t count;             /* Items in queue */
    uint32_t next_seq;          /* Sequence counter */
    pressure_state_t state;     /* Pressure state */
    pressure_stats_t stats;     /* Accounting */
    uint8_t fault_reentry;      /* Fault flags */
    uint8_t fault_overflow;
    uint8_t in_operation;       /* Atomicity guard */
    uint8_t initialized;
} pressure_queue_t;
```

---

## The Item Structure

```c
typedef struct {
    uint64_t timestamp;   /* When enqueued */
    uint64_t payload;     /* Generic data */
    uint32_t sequence;    /* Monotonic ID */
} pressure_item_t;
```

---

## Statistics for Accounting

```c
typedef struct {
    uint64_t enqueued;
    uint64_t dequeued;
    uint64_t rejected;
    uint64_t dropped_oldest;
    uint64_t dropped_newest;
    uint32_t peak_fill;
    uint32_t high_water_events;
    uint32_t critical_events;
} pressure_stats_t;
```

The accounting equation:
```
enqueued = dequeued + count + dropped_oldest + dropped_newest + rejected
```

---

## Caller-Allocated Buffer

```c
pressure_item_t buffer[64];  /* Static allocation */
pressure_init(&q, &cfg, buffer);
```

No malloc. No heap. Deterministic memory.
