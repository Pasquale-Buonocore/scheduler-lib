/**
 * @file test_scheduler.c
 * @brief Unit tests for the cooperative scheduler.
 */

#include "unity.h"

#include "scheduler/isr_buffer.h"
#include "scheduler/scheduler.h"

/** @brief Fake time source used by scheduler port stubs. */
static uint32_t fake_time_us;
/** @brief Count of idle hook calls. */
static uint32_t idle_call_count;
/** @brief Count of task A executions. */
static uint32_t task_a_calls;
/** @brief Count of task B executions. */
static uint32_t task_b_calls;
/** @brief Count of background task executions. */
static uint32_t bg_calls;
/** @brief Execution trace buffer for ordering checks. */
static uint8_t call_trace[16];
/** @brief Number of valid entries in @ref call_trace. */
static uint32_t call_trace_len;
/** @brief Optional function used by a test task to mutate fake time. */
static void (*task_a_hook)(void);

typedef struct {
    sch_event_queue_t queue;
    uint16_t storage[4];
    volatile bool hint;
    size_t max_items_per_run;
    uint32_t run_count;
    uint32_t empty_returns;
    uint32_t processed_count;
    uint32_t last_event_id;
} polling_service_t;

typedef struct {
    polling_service_t *service;
    uint16_t next_event_id;
    uint32_t produce_count;
    uint32_t first_produced_tick;
    uint32_t last_produced_tick;
} producer_task_ctx_t;

typedef struct {
    polling_service_t *service;
    uint32_t consume_count;
    uint32_t first_consumed_tick;
} consumer_task_ctx_t;
#if (SCH_ENABLE_TRACE == 1)
/** @brief Count of trace hook invocations. */
static uint32_t trace_call_count;
/** @brief Last trace event observed by test hook. */
static sch_trace_event_t last_trace_event;
#endif

/** @brief Test hook that advances fake time by one period. */
static void hook_advance_time_by_period(void) {
    fake_time_us += 100u;
}

/** @brief Port stub: return fake time. */
uint32_t sch_port_now_ticks(void) {
    return fake_time_us;
}

/** @brief Port stub: enter critical section. */
uint32_t sch_port_enter_critical(void) {
    return 0u;
}

/** @brief Port stub: exit critical section. */
void sch_port_exit_critical(uint32_t state) {
    (void)state;
}

/** @brief Port stub: count idle hook calls. */
void sch_port_idle(void) {
    idle_call_count++;
}

/**
 * @brief Periodic test task A.
 *
 * @param ctx Unused callback context.
 */
static void task_a(void *ctx) {
    (void)ctx;
    task_a_calls++;
    call_trace[call_trace_len++] = (uint8_t)'A';
    if (task_a_hook != NULL) {
        task_a_hook();
    }
}

/**
 * @brief Periodic test task B.
 *
 * @param ctx Unused callback context.
 */
static void task_b(void *ctx) {
    (void)ctx;
    task_b_calls++;
    call_trace[call_trace_len++] = (uint8_t)'B';
}

/**
 * @brief Background test task.
 *
 * @param ctx Unused callback context.
 */
static void background_task(void *ctx) {
    (void)ctx;
    bg_calls++;
    call_trace[call_trace_len++] = (uint8_t)'G';
}

static void polling_service_reset(polling_service_t *service, size_t max_items_per_run) {
    (void)sch_event_queue_init(
        &service->queue, service->storage, 4u, SCH_OVERFLOW_DROP_NEWEST);
    service->hint = false;
    service->max_items_per_run = max_items_per_run;
    service->run_count = 0u;
    service->empty_returns = 0u;
    service->processed_count = 0u;
    service->last_event_id = 0u;
}

static void polling_service_push_event(polling_service_t *service, uint16_t event_id, bool set_hint) {
    (void)sch_event_queue_push_isr(&service->queue, event_id);
    if (set_hint) {
        service->hint = true;
    }
}

static void polling_service_task(void *ctx) {
    polling_service_t *service = (polling_service_t *)ctx;
    service->run_count++;

    if (!service->hint && (sch_event_queue_size(&service->queue) == 0u)) {
        service->empty_returns++;
        return;
    }

    service->hint = false;

    uint16_t event_id = 0u;
    for (size_t i = 0u; i < service->max_items_per_run; ++i) {
        if (!sch_event_queue_pop_task(&service->queue, &event_id)) {
            break;
        }

        service->processed_count++;
        service->last_event_id = (uint32_t)event_id;
    }
}

static void producer_task(void *ctx) {
    producer_task_ctx_t *producer = (producer_task_ctx_t *)ctx;

    polling_service_push_event(producer->service, producer->next_event_id++, false);
    producer->produce_count++;
    if (producer->first_produced_tick == 0u) {
        producer->first_produced_tick = fake_time_us;
    }
    producer->last_produced_tick = fake_time_us;
}

static void consumer_task(void *ctx) {
    consumer_task_ctx_t *consumer = (consumer_task_ctx_t *)ctx;
    uint16_t event_id = 0u;

    if (sch_event_queue_pop_task(&consumer->service->queue, &event_id)) {
        consumer->consume_count++;
        if (consumer->first_consumed_tick == 0u) {
            consumer->first_consumed_tick = fake_time_us;
        }
    }
}

/** @brief Unity setup hook. */
void setUp(void) {
    fake_time_us = 0u;
    idle_call_count = 0u;
    task_a_calls = 0u;
    task_b_calls = 0u;
    bg_calls = 0u;
    call_trace_len = 0u;
    task_a_hook = NULL;
#if (SCH_ENABLE_TRACE == 1)
    trace_call_count = 0u;
    last_trace_event = SCH_TRACE_IDLE;
#endif
}

/** @brief Unity teardown hook. */
void tearDown(void) {
}

/** @brief Verify that invalid arguments are rejected by sch_add_task. */
void test_sch_add_task_should_reject_invalid_params(void) {
    sch_t scheduler;

    TEST_ASSERT_EQUAL_INT32(SCH_ERR_INVALID_ARG, sch_add_task(NULL, task_a, NULL, 1000u, 0u, 1u));
    sch_init(&scheduler);
    TEST_ASSERT_EQUAL_INT32(SCH_ERR_INVALID_ARG, sch_add_task(&scheduler, NULL, NULL, 1000u, 0u, 1u));
}

/** @brief Verify priority ordering of ready periodic tasks. */
void test_sch_run_should_execute_ready_tasks_by_priority(void) {
    sch_t scheduler;
    sch_init(&scheduler);

    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, sch_add_task(&scheduler, task_b, NULL, 1000u, 1000u, 10u));
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, sch_add_task(&scheduler, task_a, NULL, 1000u, 1000u, 0u));

    fake_time_us = 1000u;
    sch_run(&scheduler);

    TEST_ASSERT_EQUAL_UINT32(1u, task_a_calls);
    TEST_ASSERT_EQUAL_UINT32(1u, task_b_calls);
    TEST_ASSERT_EQUAL_UINT32(0u, idle_call_count);
    TEST_ASSERT_EQUAL_UINT32(2u, call_trace_len);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'A', call_trace[0]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'B', call_trace[1]);
}

/** @brief Verify bounded catch-up for delayed periodic execution. */
void test_sch_run_should_catch_up_missed_periods_without_duplicate_execution(void) {
    sch_t scheduler;
    sch_init(&scheduler);

    int32_t id = sch_add_task(&scheduler, task_a, NULL, 1000u, 1000u, 0u);
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, id);

    fake_time_us = 3500u;
    sch_run(&scheduler);
    TEST_ASSERT_EQUAL_UINT32(1u, task_a_calls);

    fake_time_us = 3999u;
    sch_run(&scheduler);
    TEST_ASSERT_EQUAL_UINT32(1u, task_a_calls);

    fake_time_us = 4000u;
    sch_run(&scheduler);
    TEST_ASSERT_EQUAL_UINT32(2u, task_a_calls);
}

/** @brief Verify one background task runs when no periodic task is ready. */
void test_sch_run_should_execute_one_background_task_when_no_periodic_ready(void) {
    sch_t scheduler;
    sch_init(&scheduler);

    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, sch_add_task(&scheduler, background_task, NULL, 0u, 0u, 20u));
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, sch_add_task(&scheduler, task_a, NULL, 1000u, 5000u, 0u));

    fake_time_us = 1000u;
    sch_run(&scheduler);

    TEST_ASSERT_EQUAL_UINT32(0u, task_a_calls);
    TEST_ASSERT_EQUAL_UINT32(1u, bg_calls);
    TEST_ASSERT_EQUAL_UINT32(0u, idle_call_count);
}

/** @brief Verify idle hook executes when no task is available. */
void test_sch_run_should_call_idle_when_nothing_to_execute(void) {
    sch_t scheduler;
    sch_init(&scheduler);

    sch_run(&scheduler);

    TEST_ASSERT_EQUAL_UINT32(1u, idle_call_count);
}

/** @brief Verify runtime enable/disable API does not affect background tasks. */
void test_sch_enable_task_should_not_disable_background_task(void) {
    sch_t scheduler;
    sch_init(&scheduler);

    int32_t id = sch_add_task(&scheduler, background_task, NULL, 0u, 0u, 0u);
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, id);

    sch_enable_task(&scheduler, (uint32_t)id, false);
    sch_run(&scheduler);
    TEST_ASSERT_EQUAL_UINT32(1u, bg_calls);
    TEST_ASSERT_EQUAL_UINT32(0u, idle_call_count);
}

/** @brief Verify periodic tasks remain enabled even if disable is requested. */
void test_sch_enable_task_should_not_disable_periodic_task(void) {
    sch_t scheduler;
    sch_init(&scheduler);

    int32_t id = sch_add_task(&scheduler, task_a, NULL, 1000u, 1000u, 0u);
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, id);

    sch_enable_task(&scheduler, (uint32_t)id, false);
    fake_time_us = 1000u;
    sch_run(&scheduler);

    TEST_ASSERT_EQUAL_UINT32(1u, task_a_calls);
}

/** @brief Verify a scheduler run executes a bounded periodic ready set once. */
void test_sch_run_should_bound_periodic_work_per_cycle(void) {
    sch_t scheduler;
    sch_init(&scheduler);

    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, sch_add_task(&scheduler, task_a, NULL, 100u, 100u, 0u));
    task_a_hook = hook_advance_time_by_period;
    fake_time_us = 100u;
    sch_run(&scheduler);
    TEST_ASSERT_EQUAL_UINT32(1u, task_a_calls);

    sch_run(&scheduler);
    TEST_ASSERT_EQUAL_UINT32(2u, task_a_calls);

    task_a_hook = NULL;
}

void test_polling_service_should_run_periodically_and_return_cleanly_when_empty(void) {
    sch_t scheduler;
    polling_service_t service;
    sch_init(&scheduler);
    polling_service_reset(&service, 2u);

    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, sch_add_task(&scheduler, polling_service_task, &service, 1000u, 1000u, 5u));

    fake_time_us = 1000u;
    sch_run(&scheduler);

    TEST_ASSERT_EQUAL_UINT32(1u, service.run_count);
    TEST_ASSERT_EQUAL_UINT32(1u, service.empty_returns);
    TEST_ASSERT_EQUAL_UINT32(0u, service.processed_count);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)sch_event_queue_size(&service.queue));

    fake_time_us = 2000u;
    sch_run(&scheduler);

    TEST_ASSERT_EQUAL_UINT32(2u, service.run_count);
    TEST_ASSERT_EQUAL_UINT32(2u, service.empty_returns);
    TEST_ASSERT_EQUAL_UINT32(0u, service.processed_count);
}

void test_polling_service_should_defer_backlog_across_periodic_runs_with_bounded_work(void) {
    sch_t scheduler;
    polling_service_t service;
    sch_init(&scheduler);
    polling_service_reset(&service, 1u);
    polling_service_push_event(&service, 10u, false);
    polling_service_push_event(&service, 11u, false);

    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, sch_add_task(&scheduler, polling_service_task, &service, 1000u, 1000u, 5u));

    fake_time_us = 1000u;
    sch_run(&scheduler);
    TEST_ASSERT_EQUAL_UINT32(1u, service.run_count);
    TEST_ASSERT_EQUAL_UINT32(1u, service.processed_count);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_event_queue_size(&service.queue));

    fake_time_us = 2000u;
    sch_run(&scheduler);
    TEST_ASSERT_EQUAL_UINT32(2u, service.run_count);
    TEST_ASSERT_EQUAL_UINT32(2u, service.processed_count);
    TEST_ASSERT_EQUAL_UINT32(11u, service.last_event_id);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)sch_event_queue_size(&service.queue));
}

void test_polling_service_should_treat_hint_as_advisory_only(void) {
    sch_t scheduler;
    polling_service_t service;
    sch_init(&scheduler);
    polling_service_reset(&service, 2u);

    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, sch_add_task(&scheduler, polling_service_task, &service, 1000u, 1000u, 5u));

    service.hint = true;
    fake_time_us = 1000u;
    sch_run(&scheduler);
    TEST_ASSERT_EQUAL_UINT32(1u, service.run_count);
    TEST_ASSERT_EQUAL_UINT32(0u, service.processed_count);
    TEST_ASSERT_FALSE(service.hint);

    polling_service_push_event(&service, 55u, false);
    fake_time_us = 2000u;
    sch_run(&scheduler);
    TEST_ASSERT_EQUAL_UINT32(2u, service.run_count);
    TEST_ASSERT_EQUAL_UINT32(1u, service.processed_count);
    TEST_ASSERT_EQUAL_UINT32(55u, service.last_event_id);
}

void test_polling_service_latency_should_be_bounded_by_one_period_when_consumer_runs_before_producer(void) {
    sch_t scheduler;
    polling_service_t service;
    producer_task_ctx_t producer = {0};
    consumer_task_ctx_t consumer = {0};

    sch_init(&scheduler);
    polling_service_reset(&service, 1u);

    producer.service = &service;
    producer.next_event_id = 1u;
    consumer.service = &service;

    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, sch_add_task(&scheduler, consumer_task, &consumer, 1000u, 1000u, 0u));
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, sch_add_task(&scheduler, producer_task, &producer, 1000u, 1000u, 1u));

    fake_time_us = 1000u;
    sch_run(&scheduler);
    TEST_ASSERT_EQUAL_UINT32(1u, producer.produce_count);
    TEST_ASSERT_EQUAL_UINT32(0u, consumer.consume_count);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_event_queue_size(&service.queue));

    fake_time_us = 2000u;
    sch_run(&scheduler);
    TEST_ASSERT_EQUAL_UINT32(2u, producer.produce_count);
    TEST_ASSERT_EQUAL_UINT32(1u, consumer.consume_count);
    TEST_ASSERT_EQUAL_UINT32(2000u, consumer.first_consumed_tick);
    TEST_ASSERT_EQUAL_UINT32(1000u, consumer.first_consumed_tick - producer.first_produced_tick);
}

#if (SCH_ENABLE_STATS == 1)
/** @brief Verify stats capture execution count and timing snapshots. */
void test_sch_stats_should_capture_task_execution_observability(void) {
    sch_t scheduler;
    sch_task_stats_t stats;
    sch_init(&scheduler);

    int32_t id = sch_add_task(&scheduler, task_a, NULL, 100u, 100u, 0u);
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, id);

    fake_time_us = 100u;
    task_a_hook = hook_advance_time_by_period;
    sch_run(&scheduler);
    task_a_hook = NULL;

    TEST_ASSERT_TRUE(sch_get_task_stats(&scheduler, (uint32_t)id, &stats));
    TEST_ASSERT_EQUAL_UINT32(1u, stats.run_count);
    TEST_ASSERT_EQUAL_UINT32(100u, stats.last_exec_ticks);
    TEST_ASSERT_EQUAL_UINT32(100u, stats.max_exec_ticks);
    TEST_ASSERT_EQUAL_UINT32(100u, stats.total_exec_ticks);

    sch_reset_stats(&scheduler);
    TEST_ASSERT_TRUE(sch_get_task_stats(&scheduler, (uint32_t)id, &stats));
    TEST_ASSERT_EQUAL_UINT32(0u, stats.run_count);
    TEST_ASSERT_EQUAL_UINT32(0u, stats.total_exec_ticks);
}
#endif

#if (SCH_ENABLE_TRACE == 1)
/** @brief Test trace hook increments invocation count. */
static void trace_hook(sch_trace_event_t event, int32_t task_id, uint32_t timestamp, void *user_ctx) {
    (void)task_id;
    (void)timestamp;
    uint32_t *count = (uint32_t *)user_ctx;
    (*count)++;
    last_trace_event = event;
}

/** @brief Verify trace callback fires for task execution and idle path. */
void test_sch_trace_should_emit_task_and_idle_events(void) {
    sch_t scheduler;
    sch_init(&scheduler);

    sch_set_trace_hook(&scheduler, trace_hook, &trace_call_count);
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, sch_add_task(&scheduler, task_a, NULL, 100u, 100u, 0u));

    fake_time_us = 100u;
    sch_run(&scheduler);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2u, trace_call_count);

    sch_set_trace_hook(&scheduler, trace_hook, &trace_call_count);
    trace_call_count = 0u;
    sch_init(&scheduler);
    sch_set_trace_hook(&scheduler, trace_hook, &trace_call_count);
    sch_run(&scheduler);
    TEST_ASSERT_EQUAL_UINT32(1u, trace_call_count);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)SCH_TRACE_IDLE, (uint32_t)last_trace_event);
}
#endif
