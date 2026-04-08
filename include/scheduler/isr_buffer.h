/**
 * @file isr_buffer.h
 * @brief Deterministic ISR-producer/task-consumer buffering utilities.
 */

#ifndef SCHEDULER_ISR_BUFFER_H_
#define SCHEDULER_ISR_BUFFER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Overflow policy for full ring buffers. */
typedef enum {
    SCH_OVERFLOW_DROP_NEWEST = 0, /**< Reject incoming item when full. */
    SCH_OVERFLOW_DROP_OLDEST = 1  /**< Discard oldest item and enqueue incoming item. */
} sch_overflow_mode_t;

/** @brief Result of a push operation. */
typedef enum {
    SCH_RING_PUSH_OK = 0,             /**< Item stored successfully. */
    SCH_RING_PUSH_DROPPED_NEWEST = 1, /**< Incoming item dropped because buffer was full. */
    SCH_RING_PUSH_DROPPED_OLDEST = 2  /**< Oldest buffered item was dropped to make room. */
} sch_ring_push_status_t;

/**
 * @brief Generic fixed-capacity ring buffer for ISR producer / task consumer.
 *
 * @note This utility is intended for one producer context (typically ISR) and
 *       one consumer context (scheduler task). Public operations guard their
 *       critical sections via scheduler port hooks for deterministic,
 *       interrupt-safe index updates.
 */
typedef struct {
    uint8_t *storage;              /**< Backing storage provided by caller. */
    size_t element_size;           /**< Size of one element in bytes. */
    size_t capacity;               /**< Number of elements in @ref storage. */
    size_t head;                   /**< Producer write index. */
    size_t tail;                   /**< Consumer read index. */
    size_t count;                  /**< Current number of buffered elements. */
    size_t drops;                  /**< Total dropped elements due to overflow. */
    sch_overflow_mode_t overflow;  /**< Overflow behavior when full. */
} sch_spsc_ring_t;

bool sch_spsc_ring_init(
    sch_spsc_ring_t *ring,
    void *storage,
    size_t capacity,
    size_t element_size,
    sch_overflow_mode_t overflow);

void sch_spsc_ring_reset(sch_spsc_ring_t *ring);

sch_ring_push_status_t sch_spsc_ring_push_isr(sch_spsc_ring_t *ring, const void *item);

bool sch_spsc_ring_pop_task(sch_spsc_ring_t *ring, void *out_item);

size_t sch_spsc_ring_size(const sch_spsc_ring_t *ring);

size_t sch_spsc_ring_drop_count(const sch_spsc_ring_t *ring);

bool sch_spsc_ring_is_empty(const sch_spsc_ring_t *ring);

bool sch_spsc_ring_is_full(const sch_spsc_ring_t *ring);

/**
 * @brief Lightweight descriptor/event queue where payload is external.
 */
typedef struct {
    sch_spsc_ring_t ring; /**< Internal ring for 16-bit event/descriptor IDs. */
} sch_event_queue_t;

bool sch_event_queue_init(
    sch_event_queue_t *queue,
    uint16_t *storage,
    size_t capacity,
    sch_overflow_mode_t overflow);

sch_ring_push_status_t sch_event_queue_push_isr(sch_event_queue_t *queue, uint16_t event_id);

bool sch_event_queue_pop_task(sch_event_queue_t *queue, uint16_t *out_event_id);

size_t sch_event_queue_size(const sch_event_queue_t *queue);

size_t sch_event_queue_drop_count(const sch_event_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_ISR_BUFFER_H_ */
