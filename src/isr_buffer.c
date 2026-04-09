/**
 * @file isr_buffer.c
 * @brief Deterministic ISR-producer/task-consumer buffering utilities.
 */

#include "scheduler/isr_buffer.h"

#include <string.h>

#include "scheduler/port/scheduler_port.h"

static bool sch_ring_valid(const sch_spsc_ring_t *ring) {
    return (ring != NULL) && (ring->storage != NULL) && (ring->capacity > 0u) &&
           (ring->element_size > 0u) &&
           ((ring->overflow == SCH_OVERFLOW_DROP_NEWEST) ||
            (ring->overflow == SCH_OVERFLOW_DROP_OLDEST));
}

bool sch_spsc_ring_init(
    sch_spsc_ring_t *ring,
    void *storage,
    size_t capacity,
    size_t element_size,
    sch_overflow_mode_t overflow) {
    if ((ring == NULL) || (storage == NULL) || (capacity == 0u) || (element_size == 0u)) {
        return false;
    }

    if ((overflow != SCH_OVERFLOW_DROP_NEWEST) && (overflow != SCH_OVERFLOW_DROP_OLDEST)) {
        return false;
    }

    uint32_t state = sch_port_enter_critical();
    ring->storage = (unsigned char *)storage;
    ring->element_size = element_size;
    ring->capacity = capacity;
    ring->head = 0u;
    ring->tail = 0u;
    ring->count = 0u;
    ring->drops = 0u;
    ring->overflow = overflow;
    sch_port_exit_critical(state);

    return true;
}

void sch_spsc_ring_reset(sch_spsc_ring_t *ring) {
    if (!sch_ring_valid(ring)) {
        return;
    }

    uint32_t state = sch_port_enter_critical();
    ring->head = 0u;
    ring->tail = 0u;
    ring->count = 0u;
    ring->drops = 0u;
    sch_port_exit_critical(state);
}

sch_ring_push_status_t sch_spsc_ring_push_isr(sch_spsc_ring_t *ring, const void *item) {
    if (!sch_ring_valid(ring) || (item == NULL)) {
        return SCH_RING_PUSH_DROPPED_NEWEST;
    }

    uint32_t state = sch_port_enter_critical();

    bool dropped_oldest = false;
    if (ring->count == ring->capacity) {
        ring->drops++;
        if (ring->overflow == SCH_OVERFLOW_DROP_NEWEST) {
            sch_port_exit_critical(state);
            return SCH_RING_PUSH_DROPPED_NEWEST;
        }

        ring->tail = (ring->tail + 1u) % ring->capacity;
        ring->count--;
        dropped_oldest = true;
    }

    unsigned char *dst = &ring->storage[ring->head * ring->element_size];
    (void)memcpy(dst, item, ring->element_size);
    ring->head = (ring->head + 1u) % ring->capacity;
    ring->count++;

    sch_ring_push_status_t status = dropped_oldest ? SCH_RING_PUSH_DROPPED_OLDEST : SCH_RING_PUSH_OK;

    sch_port_exit_critical(state);
    return status;
}

bool sch_spsc_ring_pop_task(sch_spsc_ring_t *ring, void *out_item) {
    if (!sch_ring_valid(ring) || (out_item == NULL)) {
        return false;
    }

    uint32_t state = sch_port_enter_critical();
    if (ring->count == 0u) {
        sch_port_exit_critical(state);
        return false;
    }

    unsigned char *src = &ring->storage[ring->tail * ring->element_size];
    (void)memcpy(out_item, src, ring->element_size);
    ring->tail = (ring->tail + 1u) % ring->capacity;
    ring->count--;

    sch_port_exit_critical(state);
    return true;
}

size_t sch_spsc_ring_size(const sch_spsc_ring_t *ring) {
    if (!sch_ring_valid(ring)) {
        return 0u;
    }

    uint32_t state = sch_port_enter_critical();
    size_t size = ring->count;
    sch_port_exit_critical(state);
    return size;
}

size_t sch_spsc_ring_drop_count(const sch_spsc_ring_t *ring) {
    if (!sch_ring_valid(ring)) {
        return 0u;
    }

    uint32_t state = sch_port_enter_critical();
    size_t drops = ring->drops;
    sch_port_exit_critical(state);
    return drops;
}

bool sch_spsc_ring_is_empty(const sch_spsc_ring_t *ring) {
    return (sch_spsc_ring_size(ring) == 0u);
}

bool sch_spsc_ring_is_full(const sch_spsc_ring_t *ring) {
    if (!sch_ring_valid(ring)) {
        return false;
    }

    uint32_t state = sch_port_enter_critical();
    bool full = (ring->count == ring->capacity);
    sch_port_exit_critical(state);
    return full;
}

bool sch_event_queue_init(
    sch_event_queue_t *queue,
    uint16_t *storage,
    size_t capacity,
    sch_overflow_mode_t overflow) {
    if (queue == NULL) {
        return false;
    }

    return sch_spsc_ring_init(&queue->ring, storage, capacity, sizeof(uint16_t), overflow);
}

sch_ring_push_status_t sch_event_queue_push_isr(sch_event_queue_t *queue, uint16_t event_id) {
    if (queue == NULL) {
        return SCH_RING_PUSH_DROPPED_NEWEST;
    }

    return sch_spsc_ring_push_isr(&queue->ring, &event_id);
}

bool sch_event_queue_pop_task(sch_event_queue_t *queue, uint16_t *out_event_id) {
    if (queue == NULL) {
        return false;
    }

    return sch_spsc_ring_pop_task(&queue->ring, out_event_id);
}

size_t sch_event_queue_size(const sch_event_queue_t *queue) {
    if (queue == NULL) {
        return 0u;
    }

    return sch_spsc_ring_size(&queue->ring);
}

size_t sch_event_queue_drop_count(const sch_event_queue_t *queue) {
    if (queue == NULL) {
        return 0u;
    }

    return sch_spsc_ring_drop_count(&queue->ring);
}
