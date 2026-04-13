# ISR Buffering Guide: Circular Buffer vs Descriptor/Event Queue

This document explains the two buffering utilities in `isr_buffer` and when to use each one:

- `sch_spsc_ring_t` (generic circular buffer)
- `sch_event_queue_t` (lightweight descriptor/event queue)

Both are designed for the same concurrency model: **single producer in ISR context** and **single consumer in task/service context**.

---

## 1) Mental model

### Circular buffer (`sch_spsc_ring_t`)
Use this when the ISR needs to hand off **actual payload bytes/records** to a task.

Examples:
- UART RX bytes
- Compact ISR records
- Small fixed-size telemetry samples

### Descriptor/event queue (`sch_event_queue_t`)
Use this when ISR should hand off only a **small ID/token** (a `uint16_t`) and the payload is external.

Examples:
- DMA descriptor index
- "RX ready" / "TX done" event IDs
- Completion IDs for deferred operations

In other words:
- ring buffer = "data is in the queue"
- event queue = "queue points to data/state managed elsewhere"

---

## 2) API overview

### Generic ring (`sch_spsc_ring_t`)

- `sch_spsc_ring_init(...)`
- `sch_spsc_ring_push_isr(...)`
- `sch_spsc_ring_pop_task(...)`
- `sch_spsc_ring_size(...)`
- `sch_spsc_ring_drop_count(...)`
- `sch_spsc_ring_reset(...)`

You provide:
- backing storage
- element size
- capacity
- overflow mode

### Event queue (`sch_event_queue_t`)

- `sch_event_queue_init(...)`
- `sch_event_queue_push_isr(...)`
- `sch_event_queue_pop_task(...)`
- `sch_event_queue_size(...)`
- `sch_event_queue_drop_count(...)`

This is a typed convenience wrapper around the generic ring with:
- `element_size = sizeof(uint16_t)`
- storage typed as `uint16_t *`

### Additional `isr_buffer` features worth documenting

Besides the two queue types, there are core helper features that are important for integration and observability:

- Push result status enum: `sch_ring_push_status_t`
  - `SCH_RING_PUSH_OK`
  - `SCH_RING_PUSH_DROPPED_NEWEST`
  - `SCH_RING_PUSH_DROPPED_OLDEST`
- Ring state helpers:
  - `sch_spsc_ring_is_empty(...)`
  - `sch_spsc_ring_is_full(...)`
- Lifecycle helper:
  - `sch_spsc_ring_reset(...)` resets indices and counters in a critical section.

If your service requires backpressure metrics, queue health checks, or adaptive behavior, these helpers are just as important as the core push/pop APIs.

---

## 3) Overflow behavior (applies to both)

Choose one policy at init time:

- `SCH_OVERFLOW_DROP_NEWEST`
  - New item is rejected when full.
  - Old buffered items are preserved.
- `SCH_OVERFLOW_DROP_OLDEST`
  - Oldest buffered item is discarded.
  - New incoming item is kept.

Track losses using `*_drop_count(...)`.

Guideline:
- Prefer `DROP_NEWEST` when order/history matters more than recency.
- Prefer `DROP_OLDEST` when latest state is more valuable than stale state.

---

## 4) Ownership and call-context rules

These utilities assume:
- exactly one producer (typically ISR)
- exactly one consumer (task/service)
- no blocking in ISR paths

Critical sections are used internally for deterministic index/state updates. Port hooks must be ISR-safe and preserve nesting semantics.

Do **not** call task-side scheduler execution from ISR; ISR should only acknowledge/capture/queue/hint.

---

## 5) Recommended usage pattern

### A) Circular buffer pattern (payload handoff)

1. Allocate fixed storage for payload elements.
2. Init `sch_spsc_ring_t` with capacity and element size.
3. ISR path:
   - capture minimal payload record
   - `sch_spsc_ring_push_isr(...)`
   - set wake hint/event bit
4. Task path:
   - if no hint and queue empty: return quickly
   - pop in a bounded loop (budget per run)
   - process payload records

Use this for continuous streams where every element carries meaningful data.

### B) Event queue pattern (descriptor/event handoff)

1. Allocate `uint16_t` storage.
2. Init `sch_event_queue_t` with capacity/policy.
3. ISR path:
   - ACK hardware interrupt
   - push descriptor/event ID with `sch_event_queue_push_isr(...)`
   - set a wake hint
4. Task path:
   - bounded drain loop (`max_events_per_run`)
   - pop event ID
   - call HAL/driver to fetch actual descriptor/payload/state

Use this for systems where the data already lives in hardware rings/driver state and ISR only needs to signal "what happened".

---

## 6) Choosing between ring and event queue

Use **circular buffer** when:
- you need to carry bytes/records directly
- consumer should not have to query external state for each item

Use **event queue** when:
- item can be represented by a `uint16_t` identifier
- external payload/state is already available (DMA descriptor tables, HAL state)
- you want tiny ISR handoff records

A common hybrid is:
- event bits/hints for wakeup urgency
- ring/event queue for multiplicity and ordering

---

## 7) Example mapping in this project

`ethernet_service` follows the descriptor/event queue model:
- ISR acknowledges IRQ and queues event IDs (`RX_READY`, `TX_DONE`, `LINK_CHANGE`).
- Service drains events in bounded batches.
- Service then queries HAL callbacks for descriptor completion/state work.

This is the preferred pattern for high-rate interrupts where ISR must remain minimal and deterministic.

---

## 8) Capacity and tuning checklist

- Start from worst-case burst depth between service runs.
- Set a bounded per-run drain budget (`max_events_per_run` style).
- Select overflow mode by control objective (history vs freshness).
- Monitor `drop_count` in diagnostics.
- If drops occur:
  - increase queue capacity, and/or
  - increase service drain budget/frequency, and/or
  - reduce ISR production burst size when possible.

---

## 9) Common mistakes to avoid

- Using queue APIs from multiple producers or multiple consumers.
- Doing heavy processing in ISR instead of deferring to service task.
- Ignoring drop counters in production telemetry.
- Unbounded service draining that starves other tasks.
- Choosing overflow policy without considering system-level control behavior.

---

## 10) Short rule of thumb

- If ISR needs to transfer **data** -> use `sch_spsc_ring_t`.
- If ISR needs to transfer **which data/event** -> use `sch_event_queue_t`.
