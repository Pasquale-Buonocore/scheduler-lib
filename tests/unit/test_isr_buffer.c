/**
 * @file test_isr_buffer.c
 * @brief Unit tests for ISR/task buffering utilities.
 */

#include "unity.h"

#include "scheduler/isr_buffer.h"

/** @brief Port stub: fake critical section nesting counter for coverage. */
static uint32_t critical_depth;

uint32_t sch_port_now_ticks(void) {
    return 0u;
}

uint32_t sch_port_enter_critical(void) {
    critical_depth++;
    return 0u;
}

void sch_port_exit_critical(uint32_t state) {
    (void)state;
    if (critical_depth > 0u) {
        critical_depth--;
    }
}

void sch_port_idle(void) {
}

void setUp(void) {
    critical_depth = 0u;
}

void tearDown(void) {
}

void test_ring_should_preserve_fifo_order_for_isr_push_and_task_pop(void) {
    sch_spsc_ring_t ring;
    uint16_t storage[4];
    uint16_t out = 0u;

    TEST_ASSERT_TRUE(sch_spsc_ring_init(
        &ring, storage, 4u, sizeof(uint16_t), SCH_OVERFLOW_DROP_NEWEST));

    uint16_t v1 = 11u;
    uint16_t v2 = 22u;
    uint16_t v3 = 33u;
    TEST_ASSERT_EQUAL(SCH_RING_PUSH_OK, sch_spsc_ring_push_isr(&ring, &v1));
    TEST_ASSERT_EQUAL(SCH_RING_PUSH_OK, sch_spsc_ring_push_isr(&ring, &v2));
    TEST_ASSERT_EQUAL(SCH_RING_PUSH_OK, sch_spsc_ring_push_isr(&ring, &v3));

    TEST_ASSERT_TRUE(sch_spsc_ring_pop_task(&ring, &out));
    TEST_ASSERT_EQUAL_UINT16(11u, out);
    TEST_ASSERT_TRUE(sch_spsc_ring_pop_task(&ring, &out));
    TEST_ASSERT_EQUAL_UINT16(22u, out);
    TEST_ASSERT_TRUE(sch_spsc_ring_pop_task(&ring, &out));
    TEST_ASSERT_EQUAL_UINT16(33u, out);
    TEST_ASSERT_FALSE(sch_spsc_ring_pop_task(&ring, &out));
    TEST_ASSERT_TRUE(sch_spsc_ring_is_empty(&ring));
}

void test_ring_drop_newest_should_reject_new_items_and_count_drops(void) {
    sch_spsc_ring_t ring;
    uint16_t storage[2];
    uint16_t out = 0u;

    TEST_ASSERT_TRUE(sch_spsc_ring_init(
        &ring, storage, 2u, sizeof(uint16_t), SCH_OVERFLOW_DROP_NEWEST));

    uint16_t a = 1u;
    uint16_t b = 2u;
    uint16_t c = 3u;
    TEST_ASSERT_EQUAL(SCH_RING_PUSH_OK, sch_spsc_ring_push_isr(&ring, &a));
    TEST_ASSERT_EQUAL(SCH_RING_PUSH_OK, sch_spsc_ring_push_isr(&ring, &b));
    TEST_ASSERT_EQUAL(SCH_RING_PUSH_DROPPED_NEWEST, sch_spsc_ring_push_isr(&ring, &c));

    TEST_ASSERT_TRUE(sch_spsc_ring_is_full(&ring));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_spsc_ring_drop_count(&ring));
    TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)sch_spsc_ring_size(&ring));

    TEST_ASSERT_TRUE(sch_spsc_ring_pop_task(&ring, &out));
    TEST_ASSERT_EQUAL_UINT16(1u, out);
    TEST_ASSERT_TRUE(sch_spsc_ring_pop_task(&ring, &out));
    TEST_ASSERT_EQUAL_UINT16(2u, out);
}

void test_ring_drop_oldest_should_keep_latest_items_and_count_drops(void) {
    sch_spsc_ring_t ring;
    uint16_t storage[2];
    uint16_t out = 0u;

    TEST_ASSERT_TRUE(sch_spsc_ring_init(
        &ring, storage, 2u, sizeof(uint16_t), SCH_OVERFLOW_DROP_OLDEST));

    uint16_t a = 10u;
    uint16_t b = 20u;
    uint16_t c = 30u;
    TEST_ASSERT_EQUAL(SCH_RING_PUSH_OK, sch_spsc_ring_push_isr(&ring, &a));
    TEST_ASSERT_EQUAL(SCH_RING_PUSH_OK, sch_spsc_ring_push_isr(&ring, &b));
    TEST_ASSERT_EQUAL(SCH_RING_PUSH_DROPPED_OLDEST, sch_spsc_ring_push_isr(&ring, &c));

    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_spsc_ring_drop_count(&ring));
    TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)sch_spsc_ring_size(&ring));

    TEST_ASSERT_TRUE(sch_spsc_ring_pop_task(&ring, &out));
    TEST_ASSERT_EQUAL_UINT16(20u, out);
    TEST_ASSERT_TRUE(sch_spsc_ring_pop_task(&ring, &out));
    TEST_ASSERT_EQUAL_UINT16(30u, out);
    TEST_ASSERT_FALSE(sch_spsc_ring_pop_task(&ring, &out));
}

void test_ring_reset_should_clear_contents_and_drop_counter(void) {
    sch_spsc_ring_t ring;
    uint16_t storage[3];

    TEST_ASSERT_TRUE(sch_spsc_ring_init(
        &ring, storage, 3u, sizeof(uint16_t), SCH_OVERFLOW_DROP_NEWEST));

    uint16_t value = 1u;
    (void)sch_spsc_ring_push_isr(&ring, &value);
    (void)sch_spsc_ring_push_isr(&ring, &value);
    (void)sch_spsc_ring_push_isr(&ring, &value);
    (void)sch_spsc_ring_push_isr(&ring, &value);

    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_spsc_ring_drop_count(&ring));
    sch_spsc_ring_reset(&ring);

    TEST_ASSERT_TRUE(sch_spsc_ring_is_empty(&ring));
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)sch_spsc_ring_drop_count(&ring));
}

void test_ring_should_report_empty_and_full_transitions_across_wraparound(void) {
    sch_spsc_ring_t ring;
    uint16_t storage[3];
    uint16_t out = 0u;

    TEST_ASSERT_TRUE(sch_spsc_ring_init(
        &ring, storage, 3u, sizeof(uint16_t), SCH_OVERFLOW_DROP_NEWEST));
    TEST_ASSERT_TRUE(sch_spsc_ring_is_empty(&ring));
    TEST_ASSERT_FALSE(sch_spsc_ring_is_full(&ring));

    uint16_t a = 1u;
    uint16_t b = 2u;
    uint16_t c = 3u;
    uint16_t d = 4u;
    TEST_ASSERT_EQUAL(SCH_RING_PUSH_OK, sch_spsc_ring_push_isr(&ring, &a));
    TEST_ASSERT_EQUAL(SCH_RING_PUSH_OK, sch_spsc_ring_push_isr(&ring, &b));
    TEST_ASSERT_EQUAL(SCH_RING_PUSH_OK, sch_spsc_ring_push_isr(&ring, &c));

    TEST_ASSERT_FALSE(sch_spsc_ring_is_empty(&ring));
    TEST_ASSERT_TRUE(sch_spsc_ring_is_full(&ring));

    TEST_ASSERT_TRUE(sch_spsc_ring_pop_task(&ring, &out));
    TEST_ASSERT_EQUAL_UINT16(1u, out);
    TEST_ASSERT_EQUAL(SCH_RING_PUSH_OK, sch_spsc_ring_push_isr(&ring, &d));
    TEST_ASSERT_TRUE(sch_spsc_ring_is_full(&ring));

    TEST_ASSERT_TRUE(sch_spsc_ring_pop_task(&ring, &out));
    TEST_ASSERT_EQUAL_UINT16(2u, out);
    TEST_ASSERT_TRUE(sch_spsc_ring_pop_task(&ring, &out));
    TEST_ASSERT_EQUAL_UINT16(3u, out);
    TEST_ASSERT_TRUE(sch_spsc_ring_pop_task(&ring, &out));
    TEST_ASSERT_EQUAL_UINT16(4u, out);
    TEST_ASSERT_TRUE(sch_spsc_ring_is_empty(&ring));
}

void test_event_queue_should_queue_descriptor_ids(void) {
    sch_event_queue_t queue;
    uint16_t storage[2];
    uint16_t out = 0u;

    TEST_ASSERT_TRUE(sch_event_queue_init(&queue, storage, 2u, SCH_OVERFLOW_DROP_NEWEST));

    TEST_ASSERT_EQUAL(SCH_RING_PUSH_OK, sch_event_queue_push_isr(&queue, 100u));
    TEST_ASSERT_EQUAL(SCH_RING_PUSH_OK, sch_event_queue_push_isr(&queue, 101u));
    TEST_ASSERT_EQUAL(
        SCH_RING_PUSH_DROPPED_NEWEST,
        sch_event_queue_push_isr(&queue, 102u));

    TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)sch_event_queue_size(&queue));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_event_queue_drop_count(&queue));

    TEST_ASSERT_TRUE(sch_event_queue_pop_task(&queue, &out));
    TEST_ASSERT_EQUAL_UINT16(100u, out);
    TEST_ASSERT_TRUE(sch_event_queue_pop_task(&queue, &out));
    TEST_ASSERT_EQUAL_UINT16(101u, out);
    TEST_ASSERT_FALSE(sch_event_queue_pop_task(&queue, &out));
}

void test_event_queue_drop_oldest_should_keep_most_recent_descriptors(void) {
    sch_event_queue_t queue;
    uint16_t storage[2];
    uint16_t out = 0u;

    TEST_ASSERT_TRUE(sch_event_queue_init(&queue, storage, 2u, SCH_OVERFLOW_DROP_OLDEST));

    TEST_ASSERT_EQUAL(SCH_RING_PUSH_OK, sch_event_queue_push_isr(&queue, 200u));
    TEST_ASSERT_EQUAL(SCH_RING_PUSH_OK, sch_event_queue_push_isr(&queue, 201u));
    TEST_ASSERT_EQUAL(SCH_RING_PUSH_DROPPED_OLDEST, sch_event_queue_push_isr(&queue, 202u));

    TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)sch_event_queue_size(&queue));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_event_queue_drop_count(&queue));

    TEST_ASSERT_TRUE(sch_event_queue_pop_task(&queue, &out));
    TEST_ASSERT_EQUAL_UINT16(201u, out);
    TEST_ASSERT_TRUE(sch_event_queue_pop_task(&queue, &out));
    TEST_ASSERT_EQUAL_UINT16(202u, out);
    TEST_ASSERT_FALSE(sch_event_queue_pop_task(&queue, &out));
}
