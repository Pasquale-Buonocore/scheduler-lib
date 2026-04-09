/**
 * @file scheduler.c
 * @brief Cooperative scheduler implementation.
 *
 * This translation unit implements the scheduler API declared in
 * @ref scheduler/scheduler.h and uses the target hooks provided by
 * @ref scheduler/port/scheduler_port.h.
 */

#include "scheduler/scheduler.h"

#include "scheduler/port/scheduler_port.h"

#define SCH_LOWEST_PRIORITY (UINT16_MAX)

#if (SCH_ENABLE_TRACE == 1)
/**
 * @brief Emit trace event if tracing is enabled and callback is set.
 */
static inline void sch_trace_emit(
    sch_t *scheduler, sch_trace_event_t event, int32_t task_id, uint32_t timestamp) {
    if ((scheduler != NULL) && (scheduler->trace_hook != NULL)) {
        scheduler->trace_hook(event, task_id, timestamp, scheduler->trace_user_ctx);
    }
}
#endif

#if (SCH_ENABLE_STATS == 1)
/**
 * @brief Initialize one stats slot.
 */
static inline void sch_stats_clear_slot(sch_task_stats_t *stats) {
    stats->run_count = 0u;
    stats->miss_count = 0u;
    stats->overrun_count = 0u;
    stats->last_start_tick = 0u;
    stats->last_end_tick = 0u;
    stats->last_exec_ticks = 0u;
    stats->max_exec_ticks = 0u;
    stats->total_exec_ticks = 0u;
}
#endif

/**
 * @brief Check whether a target tick has been reached.
 *
 * The comparison is wrap-around safe for the usual half-range interval
 * used by embedded tick counters.
 *
 * @param now Current tick value.
 * @param target Tick value to compare against.
 *
 * @retval true The target tick has been reached.
 * @retval false The target tick has not been reached.
 */
static bool sch_time_reached(uint32_t now, uint32_t target) {
    return ((int32_t)(now - target) >= 0);
}

/**
 * @brief Advance a periodic task release time in bounded time.
 *
 * This helper computes the first release strictly greater than @p now,
 * avoiding an unbounded catch-up loop while the scheduler is in a critical
 * section.
 *
 * @param now Current tick value.
 * @param next_release Previously scheduled release tick.
 * @param period Task period in ticks.
 *
 * @return The next release tick strictly after @p now for valid periodic
 *         tasks, or @p next_release when @p period is zero.
 */
static uint32_t sch_advance_next_release(uint32_t now, uint32_t next_release, uint32_t period) {
    if (period == 0u) {
        return next_release;
    }

    int32_t lateness = (int32_t)(now - next_release);
    if (lateness < 0) {
        return next_release;
    }

    uint32_t releases_to_skip = ((uint32_t)lateness / period) + 1u;
    return next_release + (releases_to_skip * period);
}

/**
 * @brief Initialize scheduler state.
 *
 * @param scheduler Scheduler instance to initialize.
 */
void sch_init(sch_t *scheduler) {
    if (scheduler == NULL) {
        return;
    }

    uint32_t state = sch_port_enter_critical();
    for (uint32_t i = 0u; i < SCH_MAX_TASKS; ++i) {
        scheduler->tasks[i].fn = NULL;
        scheduler->tasks[i].ctx = NULL;
        scheduler->tasks[i].period_ticks = 0u;
        scheduler->tasks[i].next_release = 0u;
        scheduler->tasks[i].priority = SCH_LOWEST_PRIORITY;
        scheduler->tasks[i].enabled = false;
        scheduler->tasks[i].pending = false;
        scheduler->tasks[i].in_use = false;
    }
    scheduler->task_count = 0u;
    scheduler->next_background_idx = 0u;
#if (SCH_ENABLE_STATS == 1)
    for (uint32_t i = 0u; i < SCH_MAX_TASKS; ++i) {
        sch_stats_clear_slot(&scheduler->stats[i]);
    }
#endif
#if (SCH_ENABLE_TRACE == 1)
    scheduler->trace_hook = NULL;
    scheduler->trace_user_ctx = NULL;
#endif
    sch_port_exit_critical(state);
}

/**
 * @brief Add a task to the scheduler.
 *
 * @param scheduler Scheduler instance.
 * @param fn Task callback.
 * @param ctx Opaque task context pointer.
 * @param period_ticks Period in ticks (0 for background tasks).
 * @param start_delay_ticks Delay before first release.
 * @param priority Lower numeric value means higher priority.
 *
 * @return Task slot index on success or a negative error code.
 */
int32_t sch_add_task(
    sch_t *scheduler,
    sch_task_fn_t fn,
    void *ctx,
    uint32_t period_ticks,
    uint32_t start_delay_ticks,
    uint16_t priority) {
    if ((scheduler == NULL) || (fn == NULL)) {
        return SCH_ERR_INVALID_ARG;
    }

    uint32_t state = sch_port_enter_critical();
    if (scheduler->task_count >= SCH_MAX_TASKS) {
        sch_port_exit_critical(state);
        return SCH_ERR_NO_SPACE;
    }

    uint32_t now = sch_port_now_ticks();
    for (uint32_t i = 0u; i < SCH_MAX_TASKS; ++i) {
        if (!scheduler->tasks[i].in_use) {
            scheduler->tasks[i].fn = fn;
            scheduler->tasks[i].ctx = ctx;
            scheduler->tasks[i].period_ticks = period_ticks;
            scheduler->tasks[i].next_release = now + start_delay_ticks;
            scheduler->tasks[i].priority = priority;
            scheduler->tasks[i].enabled = true;
            scheduler->tasks[i].pending = false;
            scheduler->tasks[i].in_use = true;
#if (SCH_ENABLE_STATS == 1)
            sch_stats_clear_slot(&scheduler->stats[i]);
#endif
            scheduler->task_count++;
            sch_port_exit_critical(state);
            return (int32_t)i;
        }
    }

    sch_port_exit_critical(state);
    return SCH_ERR_NO_SPACE;
}

/**
 * @brief Enable or disable a task.
 *
 * @param scheduler Scheduler instance.
 * @param task_id Task identifier.
 * @param enable true to enable, false to disable.
 */
void sch_enable_task(sch_t *scheduler, uint32_t task_id, bool enable) {
    (void)scheduler;
    (void)task_id;
    (void)enable;
}

/**
 * @brief Mark periodic tasks as pending when release time is reached.
 *
 * @param scheduler Scheduler instance.
 * @param now Current tick value.
 */
static void sch_mark_periodic_ready(sch_t *scheduler, uint32_t now) {
    for (uint32_t i = 0u; i < SCH_MAX_TASKS; ++i) {
        sch_task_t *task = &scheduler->tasks[i];
        if (!task->in_use || !task->enabled || (task->period_ticks == 0u)) {
            continue;
        }

        if (sch_time_reached(now, task->next_release)) {
#if (SCH_ENABLE_STATS == 1)
            if (task->pending) {
                scheduler->stats[i].miss_count++;
            }
#endif
#if (SCH_ENABLE_TRACE == 1)
            if (task->pending) {
                sch_trace_emit(scheduler, SCH_TRACE_TASK_MISS, (int32_t)i, now);
            }
#endif
            task->pending = true;
        }
    }
}

/**
 * @brief Select the highest-priority pending periodic task.
 *
 * @param scheduler Scheduler instance.
 *
 * @return Task identifier, or @ref SCH_INVALID_TASK_ID if none are pending.
 */
static int32_t sch_select_pending_task(const sch_t *scheduler) {
    int32_t selected = SCH_INVALID_TASK_ID;
    uint16_t best_priority = SCH_LOWEST_PRIORITY;

    for (uint32_t i = 0u; i < SCH_MAX_TASKS; ++i) {
        const sch_task_t *task = &scheduler->tasks[i];
        if (!task->in_use || !task->enabled || !task->pending || (task->period_ticks == 0u)) {
            continue;
        }

        if ((selected == SCH_INVALID_TASK_ID) || (task->priority < best_priority) ||
            ((task->priority == best_priority) && (i < (uint32_t)selected))) {
            selected = (int32_t)i;
            best_priority = task->priority;
        }
    }

    return selected;
}

/**
 * @brief Select the next enabled background task in round-robin order.
 *
 * @param scheduler Scheduler instance.
 *
 * @return Task identifier, or @ref SCH_INVALID_TASK_ID if none are available.
 */
static int32_t sch_select_background_task(sch_t *scheduler) {
    uint32_t start = scheduler->next_background_idx;

    for (uint32_t offset = 0u; offset < SCH_MAX_TASKS; ++offset) {
        uint32_t idx = (start + offset) % SCH_MAX_TASKS;
        sch_task_t *task = &scheduler->tasks[idx];
        if (task->in_use && task->enabled && (task->period_ticks == 0u)) {
            scheduler->next_background_idx = (idx + 1u) % SCH_MAX_TASKS;
            return (int32_t)idx;
        }
    }

    return SCH_INVALID_TASK_ID;
}

/**
 * @brief Execute one scheduler cycle.
 *
 * The function runs all currently ready periodic tasks in priority order, then
 * executes at most one background task or calls the platform idle hook.
 *
 * @param scheduler Scheduler instance.
 */
void sch_run(sch_t *scheduler) {
    if (scheduler == NULL) {
        return;
    }

    uint32_t state = sch_port_enter_critical();
    uint32_t now = sch_port_now_ticks();
    sch_mark_periodic_ready(scheduler, now);
    sch_port_exit_critical(state);

    /*
     * Process only the ready set collected at run entry. This bounds the
     * periodic workload of one scheduler cycle to at most SCH_MAX_TASKS
     * callbacks and prevents overload from turning one run into an unbounded
     * catch-up loop.
     */
    for (uint32_t executed_periodic = 0u; executed_periodic < SCH_MAX_TASKS; ++executed_periodic) {
        state = sch_port_enter_critical();
        int32_t selected = sch_select_pending_task(scheduler);

        if (selected == SCH_INVALID_TASK_ID) {
            sch_port_exit_critical(state);
            break;
        }

        sch_task_t *task = &scheduler->tasks[selected];
        task->pending = false;
        sch_task_fn_t fn = task->fn;
        void *ctx = task->ctx;
        uint32_t period = task->period_ticks;
        uint32_t next_release = task->next_release;

        task->next_release = sch_advance_next_release(now, next_release, period);

        sch_port_exit_critical(state);

#if (SCH_ENABLE_STATS == 1)
        uint32_t exec_start = sch_port_now_ticks();
        scheduler->stats[selected].last_start_tick = exec_start;
#endif
#if (SCH_ENABLE_TRACE == 1)
        sch_trace_emit(scheduler, SCH_TRACE_TASK_START, selected, sch_port_now_ticks());
#endif
        fn(ctx);

#if (SCH_ENABLE_STATS == 1)
        uint32_t exec_end = sch_port_now_ticks();
        sch_task_stats_t *stats = &scheduler->stats[selected];
        uint32_t exec_ticks = exec_end - stats->last_start_tick;

        stats->run_count++;
        stats->last_end_tick = exec_end;
        stats->last_exec_ticks = exec_ticks;
        stats->total_exec_ticks += exec_ticks;
        if (exec_ticks > stats->max_exec_ticks) {
            stats->max_exec_ticks = exec_ticks;
        }
#endif
#if (SCH_ENABLE_TRACE == 1)
        uint32_t trace_end = sch_port_now_ticks();
        sch_trace_emit(scheduler, SCH_TRACE_TASK_END, selected, trace_end);
#endif

        /*
         * Overrun detection compares task completion time against the release
         * planned immediately after this execution.
         */
        uint32_t completed_at = sch_port_now_ticks();
        if (period != 0u) {
            if (sch_time_reached(completed_at, task->next_release)) {
#if (SCH_ENABLE_STATS == 1)
                scheduler->stats[selected].overrun_count++;
#endif
#if (SCH_ENABLE_TRACE == 1)
                sch_trace_emit(scheduler, SCH_TRACE_TASK_OVERRUN, selected, completed_at);
#endif
            }
        }
    }

    uint32_t background_state = sch_port_enter_critical();
    int32_t background_id = sch_select_background_task(scheduler);
    sch_task_fn_t background_fn = NULL;
    void *background_ctx = NULL;

    if (background_id != SCH_INVALID_TASK_ID) {
        background_fn = scheduler->tasks[background_id].fn;
        background_ctx = scheduler->tasks[background_id].ctx;
    }
    sch_port_exit_critical(background_state);

    if (background_fn != NULL) {
#if (SCH_ENABLE_TRACE == 1)
        sch_trace_emit(scheduler, SCH_TRACE_TASK_START, background_id, sch_port_now_ticks());
#endif
#if (SCH_ENABLE_STATS == 1)
        uint32_t exec_start = sch_port_now_ticks();
        scheduler->stats[background_id].last_start_tick = exec_start;
#endif
        background_fn(background_ctx);
#if (SCH_ENABLE_STATS == 1)
        uint32_t exec_end = sch_port_now_ticks();
        sch_task_stats_t *stats = &scheduler->stats[background_id];
        uint32_t exec_ticks = exec_end - stats->last_start_tick;

        stats->run_count++;
        stats->last_end_tick = exec_end;
        stats->last_exec_ticks = exec_ticks;
        stats->total_exec_ticks += exec_ticks;
        if (exec_ticks > stats->max_exec_ticks) {
            stats->max_exec_ticks = exec_ticks;
        }
#endif
#if (SCH_ENABLE_TRACE == 1)
        sch_trace_emit(scheduler, SCH_TRACE_TASK_END, background_id, sch_port_now_ticks());
#endif
    } else {
#if (SCH_ENABLE_TRACE == 1)
        sch_trace_emit(scheduler, SCH_TRACE_IDLE, SCH_INVALID_TASK_ID, sch_port_now_ticks());
#endif
        sch_port_idle();
    }
}

#if (SCH_ENABLE_STATS == 1)
void sch_reset_stats(sch_t *scheduler) {
    if (scheduler == NULL) {
        return;
    }

    uint32_t state = sch_port_enter_critical();
    for (uint32_t i = 0u; i < SCH_MAX_TASKS; ++i) {
        sch_stats_clear_slot(&scheduler->stats[i]);
    }
    sch_port_exit_critical(state);
}

bool sch_get_task_stats(const sch_t *scheduler, uint32_t task_id, sch_task_stats_t *out_stats) {
    if ((scheduler == NULL) || (out_stats == NULL) || (task_id >= SCH_MAX_TASKS) ||
        !scheduler->tasks[task_id].in_use) {
        return false;
    }

    *out_stats = scheduler->stats[task_id];
    return true;
}
#endif

#if (SCH_ENABLE_TRACE == 1)
void sch_set_trace_hook(sch_t *scheduler, sch_trace_hook_t hook, void *user_ctx) {
    if (scheduler == NULL) {
        return;
    }

    uint32_t state = sch_port_enter_critical();
    scheduler->trace_hook = hook;
    scheduler->trace_user_ctx = user_ctx;
    sch_port_exit_critical(state);
}
#endif
