# c-from-scratch — Module 6: Pressure

## Lesson 4: Code as Mathematical Transcription

---

## Enqueue Implementation

```c
int pressure_enqueue(pressure_queue_t *q, uint64_t payload, 
                     uint64_t timestamp, pressure_result_t *result)
{
    /* Reentrancy guard */
    if (q->in_operation) {
        q->fault_reentry = 1;
        return PRESSURE_ERR_REENTRY;
    }
    q->in_operation = 1;

    /* Full queue - apply policy */
    if (q->count >= q->cfg.capacity) {
        switch (q->cfg.policy) {
            case POLICY_REJECT:
                q->stats.rejected++;
                q->in_operation = 0;
                return PRESSURE_ERR_FULL;

            case POLICY_DROP_OLDEST:
                q->head = (q->head + 1) % q->cfg.capacity;
                q->count--;
                q->stats.dropped_oldest++;
                break;

            case POLICY_DROP_NEWEST:
                q->stats.dropped_newest++;
                result->was_dropped = 1;
                q->in_operation = 0;
                return PRESSURE_OK;
        }
    }

    /* Write to tail */
    q->buffer[q->tail].payload = payload;
    q->buffer[q->tail].timestamp = timestamp;
    q->buffer[q->tail].sequence = q->next_seq++;

    /* Advance tail with wraparound */
    q->tail = (q->tail + 1) % q->cfg.capacity;
    q->count++;
    q->stats.enqueued++;

    /* Update state */
    update_state(q);

    q->in_operation = 0;
    return PRESSURE_OK;
}
```

---

## Dequeue Implementation

```c
int pressure_dequeue(pressure_queue_t *q, pressure_item_t *item,
                     pressure_result_t *result)
{
    if (q->count == 0) {
        return PRESSURE_ERR_EMPTY;
    }

    /* Read from head */
    *item = q->buffer[q->head];

    /* Advance head with wraparound */
    q->head = (q->head + 1) % q->cfg.capacity;
    q->count--;
    q->stats.dequeued++;

    update_state(q);
    return PRESSURE_OK;
}
```

---

## Key Points

1. Modulo arithmetic for wraparound
2. count disambiguates full vs empty
3. Statistics track everything
4. Reentrancy guard for atomicity
