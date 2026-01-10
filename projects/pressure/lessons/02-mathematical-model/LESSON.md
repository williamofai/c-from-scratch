# c-from-scratch — Module 6: Pressure

## Lesson 2: The Mathematical Model

---

## The Ring Buffer

A ring buffer of capacity N is a fixed-size circular array where indices wrap around:

```
index' = index mod N
```

---

## State Definition

```
Sₜ = (head, tail, count, buffer[N])
```

| Symbol | Meaning | Range |
|--------|---------|-------|
| head | Read position | [0, N-1] |
| tail | Write position | [0, N-1] |
| count | Items in queue | [0, N] |
| buffer | Storage array | N items |

---

## Operations

### Enqueue (Write)

```
buffer[tail] = item
tail = (tail + 1) mod N
count = count + 1
```

Precondition: count < N (or apply overflow policy)

### Dequeue (Read)

```
item = buffer[head]
head = (head + 1) mod N
count = count - 1
```

Precondition: count > 0

---

## Overflow Policies (Formal)

When count == N and enqueue is called:

**REJECT:**
```
return ERR_FULL
count unchanged
```

**DROP_OLDEST:**
```
head = (head + 1) mod N  // Discard oldest
count = count - 1        // Then proceed with normal enqueue
```

**DROP_NEWEST:**
```
return OK
was_dropped = true
count unchanged
```

---

## Pressure States

Based on fill ratio r = count / N:

| State | Condition |
|-------|-----------|
| LOW | r < low_water / N |
| NORMAL | low_water/N ≤ r < high_water/N |
| HIGH | high_water/N ≤ r < critical_water/N |
| CRITICAL | r ≥ critical_water/N |

---

## Invariants

```
INV-1: count ≤ N (always)
INV-2: 0 ≤ head < N (always)
INV-3: 0 ≤ tail < N (always)
INV-4: enqueued = dequeued + count + dropped
```

---

## Complexity

| Operation | Time | Space |
|-----------|------|-------|
| Enqueue | O(1) | O(1) |
| Dequeue | O(1) | O(1) |
| Peek | O(1) | O(1) |
| Total | - | O(N) |

---

## Bridge to Lesson 3

Next: Encode this in C structs.
