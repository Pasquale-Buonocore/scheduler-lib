/**
 * @file test_scheduler.c
 * @brief Unit tests for the cooperative scheduler.
 */

#include "unity.h"

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

/** @brief Unity setup hook. */
void setUp(void) {
    fake_time_us = 0u;
    idle_call_count = 0u;
    task_a_calls = 0u;
    task_b_calls = 0u;
    bg_calls = 0u;
    call_trace_len = 0u;
    task_a_hook = NULL;
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
