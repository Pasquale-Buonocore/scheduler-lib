#include "scheduler/services/motor_supervision_service.h"

bool sch_motor_supervision_service_init(
    sch_motor_supervision_service_t *service,
    const sch_motor_hal_t *hal,
    sch_motor_feedback_t *feedback_storage,
    size_t feedback_capacity,
    uint16_t *event_storage,
    size_t event_capacity,
    size_t max_feedback_items_per_run,
    size_t max_events_per_run) {
    if ((service == NULL) || (hal == NULL) || (hal->set_enable == NULL) ||
        (hal->set_target_speed_rpm == NULL) || (max_feedback_items_per_run == 0u) ||
        (max_events_per_run == 0u)) {
        return false;
    }

    service->hal = *hal;
    service->target_speed_rpm = 0u;
    service->enabled = false;
    service->irq_hint = false;
    service->max_feedback_items_per_run = max_feedback_items_per_run;
    service->max_events_per_run = max_events_per_run;

    bool ok_feedback = sch_spsc_ring_init(
        &service->feedback_ring,
        feedback_storage,
        feedback_capacity,
        sizeof(sch_motor_feedback_t),
        SCH_OVERFLOW_DROP_OLDEST);
    bool ok_events = sch_event_queue_init(
        &service->event_queue, event_storage, event_capacity, SCH_OVERFLOW_DROP_OLDEST);

    return (ok_feedback && ok_events);
}

void sch_motor_supervision_set_command(
    sch_motor_supervision_service_t *service,
    bool enable,
    uint16_t target_speed_rpm) {
    if (service == NULL) {
        return;
    }

    service->enabled = enable;
    service->target_speed_rpm = target_speed_rpm;
}

void sch_motor_isr_feedback(sch_motor_supervision_service_t *service) {
    if (service == NULL) {
        return;
    }

    if (service->hal.ack_irq != NULL) {
        service->hal.ack_irq(service->hal.hal_ctx);
    }

    sch_motor_feedback_t sample = {0u, 0};
    if ((service->hal.read_feedback_sample != NULL) &&
        service->hal.read_feedback_sample(service->hal.hal_ctx, &sample)) {
        (void)sch_spsc_ring_push_isr(&service->feedback_ring, &sample);
    }

    (void)sch_event_queue_push_isr(&service->event_queue, (uint16_t)SCH_MOTOR_EVENT_FEEDBACK);
    service->irq_hint = true;
}

void sch_motor_isr_fault(sch_motor_supervision_service_t *service) {
    if (service == NULL) {
        return;
    }

    if (service->hal.ack_irq != NULL) {
        service->hal.ack_irq(service->hal.hal_ctx);
    }

    (void)sch_event_queue_push_isr(&service->event_queue, (uint16_t)SCH_MOTOR_EVENT_FAULT);
    service->irq_hint = true;
}

void sch_motor_supervision_service_run(void *ctx) {
    sch_motor_supervision_service_t *service = (sch_motor_supervision_service_t *)ctx;
    if ((service == NULL) ||
        (!service->irq_hint && sch_spsc_ring_is_empty(&service->feedback_ring) &&
         (sch_event_queue_size(&service->event_queue) == 0u))) {
        return;
    }

    service->irq_hint = false;

    (void)service->hal.set_enable(service->hal.hal_ctx, service->enabled);
    if (service->enabled) {
        (void)service->hal.set_target_speed_rpm(service->hal.hal_ctx, service->target_speed_rpm);
    }

    uint16_t event_id = 0u;
    for (size_t i = 0u; i < service->max_events_per_run; ++i) {
        if (!sch_event_queue_pop_task(&service->event_queue, &event_id)) {
            break;
        }

        if ((sch_motor_event_id_t)event_id == SCH_MOTOR_EVENT_FAULT) {
            service->enabled = false;
            (void)service->hal.set_enable(service->hal.hal_ctx, false);
        }
    }

    sch_motor_feedback_t feedback;
    for (size_t i = 0u; i < service->max_feedback_items_per_run; ++i) {
        if (!sch_spsc_ring_pop_task(&service->feedback_ring, &feedback)) {
            break;
        }

        (void)feedback;
    }
}
