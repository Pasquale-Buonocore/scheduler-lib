/**
 * @file motor_supervision_service.h
 * @brief Motor command/feedback supervision service API.
 */

#ifndef SCHEDULER_SERVICES_MOTOR_SUPERVISION_SERVICE_H_
#define SCHEDULER_SERVICES_MOTOR_SUPERVISION_SERVICE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "scheduler/isr_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Motor supervision event IDs queued from ISR context. */
typedef enum {
    SCH_MOTOR_EVENT_FEEDBACK = 1,
    SCH_MOTOR_EVENT_FAULT = 2
} sch_motor_event_id_t;

/** @brief Motor feedback sample captured from HAL. */
typedef struct {
    uint16_t speed_rpm;
    int16_t current_ma;
} sch_motor_feedback_t;

/** @brief HAL extension points for motor control and sampling. */
typedef struct {
    void (*ack_irq)(void *hal_ctx);
    bool (*read_feedback_sample)(void *hal_ctx, sch_motor_feedback_t *out_feedback);
    bool (*set_enable)(void *hal_ctx, bool enable);
    bool (*set_target_speed_rpm)(void *hal_ctx, uint16_t target_speed_rpm);
    void *hal_ctx;
} sch_motor_hal_t;

/** @brief Motor supervision service state. */
typedef struct {
    sch_motor_hal_t hal;
    sch_spsc_ring_t feedback_ring;
    sch_event_queue_t event_queue;
    uint16_t target_speed_rpm;
    bool enabled;
    /** ISR/task wake hint; set/read/clear must be inside sch_port critical sections. */
    volatile bool irq_hint;
    size_t max_feedback_items_per_run;
    size_t max_events_per_run;
} sch_motor_supervision_service_t;

/**
 * @brief Initialize motor supervision queues and command state.
 *
 * Copies HAL callbacks/context, resets command state, and initializes
 * feedback/event handoff queues.
 *
 * @param service Service instance to initialize.
 * @param hal Motor HAL callback table and opaque HAL context.
 * @param feedback_storage Caller-provided storage for feedback samples.
 * @param feedback_capacity Number of feedback samples available in storage.
 * @param event_storage Caller-provided storage for motor event IDs.
 * @param event_capacity Number of event IDs available in storage.
 * @param max_feedback_items_per_run Upper bound on feedback samples drained per run.
 * @param max_events_per_run Upper bound on motor events drained per run.
 *
 * @retval true Initialization succeeded.
 * @retval false Invalid arguments or queue initialization failed.
 */
bool sch_motor_supervision_service_init(
    sch_motor_supervision_service_t *service,
    const sch_motor_hal_t *hal,
    sch_motor_feedback_t *feedback_storage,
    size_t feedback_capacity,
    uint16_t *event_storage,
    size_t event_capacity,
    size_t max_feedback_items_per_run,
    size_t max_events_per_run);

/**
 * @brief Update desired motor enable and target-speed command.
 *
 * Command is applied by @ref sch_motor_supervision_service_run in task context.
 *
 * @param service Service instance.
 * @param enable Desired motor enable state.
 * @param target_speed_rpm Desired target speed in RPM.
 */
void sch_motor_supervision_set_command(
    sch_motor_supervision_service_t *service,
    bool enable,
    uint16_t target_speed_rpm);

/**
 * @brief Feedback ISR hook; captures sample and queues feedback event.
 *
 * @param service Service instance.
 */
void sch_motor_isr_feedback(sch_motor_supervision_service_t *service);
/**
 * @brief Fault ISR hook; queues fault event for deferred handling.
 *
 * @param service Service instance.
 */
void sch_motor_isr_fault(sch_motor_supervision_service_t *service);

/**
 * @brief Execute one bounded motor supervision cycle.
 *
 * Applies latest commands, consumes queued events, and drains feedback samples
 * with per-run limits for deterministic scheduling.
 *
 * @param ctx Pointer to @ref sch_motor_supervision_service_t.
 */
void sch_motor_supervision_service_run(void *ctx);

#ifdef __cplusplus
}
#endif /* SCHEDULER_SERVICES_MOTOR_SUPERVISION_SERVICE_H_ */

#endif
