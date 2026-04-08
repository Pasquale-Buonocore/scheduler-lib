#ifndef SCHEDULER_SERVICES_MOTOR_SUPERVISION_SERVICE_H_
#define SCHEDULER_SERVICES_MOTOR_SUPERVISION_SERVICE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "scheduler/isr_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SCH_MOTOR_EVENT_FEEDBACK = 1,
    SCH_MOTOR_EVENT_FAULT = 2
} sch_motor_event_id_t;

typedef struct {
    uint16_t speed_rpm;
    int16_t current_ma;
} sch_motor_feedback_t;

typedef struct {
    void (*ack_irq)(void *hal_ctx);
    bool (*read_feedback_sample)(void *hal_ctx, sch_motor_feedback_t *out_feedback);
    bool (*set_enable)(void *hal_ctx, bool enable);
    bool (*set_target_speed_rpm)(void *hal_ctx, uint16_t target_speed_rpm);
    void *hal_ctx;
} sch_motor_hal_t;

typedef struct {
    sch_motor_hal_t hal;
    sch_spsc_ring_t feedback_ring;
    sch_event_queue_t event_queue;
    uint16_t target_speed_rpm;
    bool enabled;
    volatile bool irq_hint;
    size_t max_feedback_items_per_run;
    size_t max_events_per_run;
} sch_motor_supervision_service_t;

bool sch_motor_supervision_service_init(
    sch_motor_supervision_service_t *service,
    const sch_motor_hal_t *hal,
    sch_motor_feedback_t *feedback_storage,
    size_t feedback_capacity,
    uint16_t *event_storage,
    size_t event_capacity,
    size_t max_feedback_items_per_run,
    size_t max_events_per_run);

void sch_motor_supervision_set_command(
    sch_motor_supervision_service_t *service,
    bool enable,
    uint16_t target_speed_rpm);

void sch_motor_isr_feedback(sch_motor_supervision_service_t *service);
void sch_motor_isr_fault(sch_motor_supervision_service_t *service);

void sch_motor_supervision_service_run(void *ctx);

#ifdef __cplusplus
}
#endif

#endif
