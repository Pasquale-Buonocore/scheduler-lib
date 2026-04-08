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
 * @brief Cooperative task callback type.
 *
 * @param ctx Opaque caller-provided context.
 */
typedef void (*sch_task_fn_t)(void *ctx);
/**
 * @brief Task control block used internally by the scheduler.
 */
typedef struct {
    sch_task_fn_t fn;      /**< Callback function for this task. */
    void *ctx;             /**< Opaque context pointer for @ref fn. */
    uint32_t period_ticks; /**< Execution period in ticks; 0 for background task. */
    uint32_t next_release; /**< Next release time in ticks for periodic tasks. */
    uint8_t priority;      /**< Lower numeric value means higher priority. */
    bool enabled;          /**< Enables or disables the task without removing it. */
    bool pending;          /**< Internal ready flag for periodic tasks. */
    bool in_use;           /**< Indicates whether this slot is allocated. */
} sch_task_t;

/**
 * @brief Scheduler instance containing static task storage.
 */
typedef struct {
    sch_task_t tasks[SCH_MAX_TASKS]; /**< Static task storage. */
    uint32_t task_count;             /**< Number of allocated task slots. */
    uint32_t next_background_idx;    /**< Round-robin cursor for background tasks. */
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
 * @param priority Lower numeric value means higher priority.
 *
 * @return Task identifier on success or negative error code on failure.
 */
int32_t sch_add_task(
    sch_t *scheduler,
    sch_task_fn_t fn,
    void *ctx,
    uint32_t period_ticks,
    uint32_t start_delay_ticks,
    uint8_t priority);

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

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_SCHEDULER_H_ */
