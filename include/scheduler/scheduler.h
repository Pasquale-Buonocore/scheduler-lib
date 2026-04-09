/**
 * @file scheduler.h
 * @brief Public API for the cooperative scheduler.
 */

#ifndef SCHEDULER_SCHEDULER_H_
#define SCHEDULER_SCHEDULER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Maximum number of task slots in a scheduler instance. */
#define SCH_MAX_TASKS (64u)

/** @brief Sentinel task identifier used when no valid task is available. */
#define SCH_INVALID_TASK_ID (-1)

/** @brief Error code returned when function arguments are invalid. */
#define SCH_ERR_INVALID_ARG (-2)

/** @brief Error code returned when no free task slots are available. */
#define SCH_ERR_NO_SPACE (-3)

/**
 * @brief Compile-time gate for scheduler stats instrumentation.
 *
 * Set to 1 (for example via compiler `-DSCH_ENABLE_STATS=1`) to enable
 * per-task scheduler counters/timestamps.
 */
#ifndef SCH_ENABLE_STATS
#define SCH_ENABLE_STATS (0)
#endif

/**
 * @brief Compile-time gate for scheduler trace callbacks.
 *
 * Set to 1 (for example via compiler `-DSCH_ENABLE_TRACE=1`) to enable
 * trace callback hooks around task execution and scheduler idle transitions.
 */
#ifndef SCH_ENABLE_TRACE
#define SCH_ENABLE_TRACE (0)
#endif

/**
 * @brief Cooperative task callback type.
 *
 * @param ctx Opaque caller-provided context.
 */
typedef void (*sch_task_fn_t)(void *ctx);

#if (SCH_ENABLE_TRACE == 1)
/**
 * @brief Scheduler trace event identifiers.
 */
typedef enum {
    SCH_TRACE_TASK_START = 0, /**< A task is about to execute. */
    SCH_TRACE_TASK_END,       /**< A task has completed execution. */
    SCH_TRACE_TASK_MISS,      /**< A periodic release was missed while already pending. */
    SCH_TRACE_TASK_OVERRUN,   /**< Task completion occurred after its next release point. */
    SCH_TRACE_IDLE            /**< Scheduler entered idle path. */
} sch_trace_event_t;

/**
 * @brief Scheduler trace callback type.
 *
 * @param event Trace event identifier.
 * @param task_id Task identifier, or @ref SCH_INVALID_TASK_ID for idle.
 * @param timestamp Tick timestamp associated with the event.
 * @param user_ctx Opaque user context supplied at trace callback registration.
 */
typedef void (*sch_trace_hook_t)(sch_trace_event_t event, int32_t task_id, uint32_t timestamp, void *user_ctx);
#endif

/**
 * @brief Task control block used internally by the scheduler.
 */
typedef struct {
    sch_task_fn_t fn;      /**< Callback function for this task. */
    void *ctx;             /**< Opaque context pointer for @ref fn. */
    uint32_t period_ticks; /**< Execution period in ticks; 0 for background task. */
    uint32_t next_release; /**< Next release time in ticks for periodic tasks. */
    uint16_t priority;     /**< 16-bit priority where lower numeric value means higher priority. */
    bool enabled;          /**< Enables or disables the task without removing it. */
    bool pending;          /**< Internal ready flag for periodic tasks. */
    bool in_use;           /**< Indicates whether this slot is allocated. */
} sch_task_t;

#if (SCH_ENABLE_STATS == 1)
/**
 * @brief Per-task scheduler instrumentation counters and timing snapshots.
 */
typedef struct {
    uint32_t run_count;         /**< Number of completed callback executions. */
    uint32_t miss_count;        /**< Number of periodic releases missed while pending. */
    uint32_t overrun_count;     /**< Number of executions finishing after next release time. */
    uint32_t last_start_tick;   /**< Tick at which the last execution started. */
    uint32_t last_end_tick;     /**< Tick at which the last execution ended. */
    uint32_t last_exec_ticks;   /**< Duration in ticks of last execution. */
    uint32_t max_exec_ticks;    /**< Maximum observed execution duration in ticks. */
    uint32_t total_exec_ticks;  /**< Accumulated execution duration in ticks. */
} sch_task_stats_t;
#endif

/**
 * @brief Scheduler instance containing static task storage.
 */
typedef struct {
    sch_task_t tasks[SCH_MAX_TASKS]; /**< Static task storage. */
    uint32_t task_count;             /**< Number of allocated task slots. */
    uint32_t next_background_idx;    /**< Round-robin cursor for background tasks. */
#if (SCH_ENABLE_STATS == 1)
    sch_task_stats_t stats[SCH_MAX_TASKS]; /**< Optional per-task instrumentation state. */
#endif
#if (SCH_ENABLE_TRACE == 1)
    sch_trace_hook_t trace_hook; /**< Optional trace callback function. */
    void *trace_user_ctx;        /**< Opaque context passed to @ref trace_hook. */
#endif
} sch_t;

/**
 * @brief Initialize a scheduler instance.
 *
 * @param scheduler Scheduler instance to initialize.
 */
void sch_init(sch_t *scheduler);

/**
 * @brief Add a task to a scheduler instance.
 *
 * @param scheduler Scheduler instance.
 * @param fn Task callback function.
 * @param ctx Opaque callback context pointer.
 * @param period_ticks Period in ticks (0 for background tasks).
 * @param start_delay_ticks Delay before first release for periodic tasks.
 * @param priority 16-bit priority where lower numeric value means higher priority.
 *
 * @return Task identifier on success or negative error code on failure.
 */
int32_t sch_add_task(
    sch_t *scheduler,
    sch_task_fn_t fn,
    void *ctx,
    uint32_t period_ticks,
    uint32_t start_delay_ticks,
    uint16_t priority);

/**
 * @brief Compatibility API: task enable/disable runtime control is unsupported.
 *
 * @param scheduler Scheduler instance.
 * @param task_id Task identifier returned by @ref sch_add_task.
 * @param enable Unused.
 */
void sch_enable_task(sch_t *scheduler, uint32_t task_id, bool enable);

/**
 * @brief Execute one cooperative scheduling cycle.
 *
 * @param scheduler Scheduler instance.
 */
void sch_run(sch_t *scheduler);

#if (SCH_ENABLE_STATS == 1)
/**
 * @brief Reset all per-task scheduler stats.
 *
 * @param scheduler Scheduler instance.
 */
void sch_reset_stats(sch_t *scheduler);

/**
 * @brief Read stats snapshot for a task.
 *
 * @param scheduler Scheduler instance.
 * @param task_id Task identifier returned by @ref sch_add_task.
 * @param out_stats Output pointer written on success.
 *
 * @retval true Stats were written to @p out_stats.
 * @retval false Invalid arguments or task slot not allocated.
 */
bool sch_get_task_stats(const sch_t *scheduler, uint32_t task_id, sch_task_stats_t *out_stats);
#endif

#if (SCH_ENABLE_TRACE == 1)
/**
 * @brief Register or clear a scheduler trace callback.
 *
 * Passing a NULL callback disables trace emission.
 *
 * Reentrancy contract:
 * - Hook executes synchronously from @ref sch_run context.
 * - Hook must not call back into scheduler APIs.
 * - Hook should be bounded and non-blocking.
 *
 * @param scheduler Scheduler instance.
 * @param hook Trace callback, or NULL to disable tracing.
 * @param user_ctx Opaque user context passed to callback.
 */
void sch_set_trace_hook(sch_t *scheduler, sch_trace_hook_t hook, void *user_ctx);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_SCHEDULER_H_ */
