# c-from-scratch — Module 6: Pressure

## Lesson 5: Testing & Verification

---

## Contract Tests

### CONTRACT-1: Bounded Memory

```c
for (int i = 0; i < 1000; i++) {
    pressure_enqueue(&q, i, i, NULL);
    assert(q.count <= cfg.capacity);  /* Always holds */
}
```

### CONTRACT-2: No Data Loss

```c
pressure_stats_t s;
pressure_get_stats(&q, &s);
uint64_t accounted = s.dequeued + q.count + 
                     s.dropped_oldest + s.dropped_newest + s.rejected;
assert(s.enqueued == accounted);
```

### CONTRACT-3: FIFO Ordering

```c
for (int i = 0; i < 10; i++) enqueue(i);
for (int i = 0; i < 10; i++) {
    dequeue(&item);
    assert(item.payload == i);  /* In order */
}
```

### CONTRACT-4: Pressure Signal

```c
/* Fill to 50% */
assert(state == PRESSURE_NORMAL);
/* Fill to 80% */
assert(state == PRESSURE_HIGH);
/* Fill to 95% */
assert(state == PRESSURE_CRITICAL);
```

---

## Invariant Tests

- count ≤ capacity (always)
- head, tail < capacity (always)
- Sequence numbers are monotonic

---

## Policy Tests

- REJECT: Returns ERR_FULL, rejects counted
- DROP_OLDEST: Overwrites oldest, tracks dropped
- DROP_NEWEST: Silently drops, was_dropped flag set

---

## Fuzz Testing

```c
for (int i = 0; i < 100000; i++) {
    if (rand() % 10 < 7) enqueue();
    else dequeue();
    assert_invariants();
}
```
