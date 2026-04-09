#include "scheduler/services/i2c_eeprom_service.h"
#include "scheduler/port/scheduler_port.h"

bool sch_i2c_eeprom_service_init(
    sch_i2c_eeprom_service_t *service,
    const sch_i2c_eeprom_hal_t *hal,
    sch_eeprom_request_t *request_storage,
    size_t request_capacity,
    uint16_t *completion_storage,
    size_t completion_capacity,
    size_t max_requests_per_run,
    size_t max_completions_per_run) {
    if ((service == NULL) || (hal == NULL) || (hal->submit_request == NULL) ||
        (max_requests_per_run == 0u) || (max_completions_per_run == 0u)) {
        return false;
    }

    service->hal = *hal;
    service->irq_hint = false;
    service->max_requests_per_run = max_requests_per_run;
    service->max_completions_per_run = max_completions_per_run;

    bool ok_requests = sch_spsc_ring_init(
        &service->request_ring,
        request_storage,
        request_capacity,
        sizeof(sch_eeprom_request_t),
        SCH_OVERFLOW_DROP_NEWEST);
    bool ok_completions = sch_event_queue_init(
        &service->completion_queue,
        completion_storage,
        completion_capacity,
        SCH_OVERFLOW_DROP_OLDEST);

    return (ok_requests && ok_completions);
}

bool sch_i2c_eeprom_enqueue_request_isr(
    sch_i2c_eeprom_service_t *service,
    const sch_eeprom_request_t *request) {
    if ((service == NULL) || (request == NULL)) {
        return false;
    }

    return (sch_spsc_ring_push_isr(&service->request_ring, request) == SCH_RING_PUSH_OK);
}

void sch_i2c_eeprom_isr_complete(sch_i2c_eeprom_service_t *service) {
    if (service == NULL) {
        return;
    }

    if (service->hal.ack_irq != NULL) {
        service->hal.ack_irq(service->hal.hal_ctx);
    }

    sch_i2c_event_id_t event_id = SCH_I2C_EVENT_TRANSFER_DONE;
    if (service->hal.consume_completion != NULL) {
        (void)service->hal.consume_completion(service->hal.hal_ctx, &event_id);
    }

    (void)sch_event_queue_push_isr(&service->completion_queue, (uint16_t)event_id);
    uint32_t state = sch_port_enter_critical();
    service->irq_hint = true;
    sch_port_exit_critical(state);
}

void sch_i2c_eeprom_service_run(void *ctx) {
    sch_i2c_eeprom_service_t *service = (sch_i2c_eeprom_service_t *)ctx;
    if (service == NULL) {
        return;
    }

    bool had_irq_hint = false;
    uint32_t state = sch_port_enter_critical();
    had_irq_hint = service->irq_hint;
    service->irq_hint = false;
    sch_port_exit_critical(state);

    if (!had_irq_hint && sch_spsc_ring_is_empty(&service->request_ring) &&
        (sch_event_queue_size(&service->completion_queue) == 0u)) {
        return;
    }

    sch_eeprom_request_t request;
    for (size_t i = 0u; i < service->max_requests_per_run; ++i) {
        if (!sch_spsc_ring_pop_task(&service->request_ring, &request)) {
            break;
        }

        if (!service->hal.submit_request(service->hal.hal_ctx, &request)) {
            break;
        }
    }

    uint16_t completion = 0u;
    for (size_t i = 0u; i < service->max_completions_per_run; ++i) {
        if (!sch_event_queue_pop_task(&service->completion_queue, &completion)) {
            break;
        }

        switch ((sch_i2c_event_id_t)completion) {
            case SCH_I2C_EVENT_TRANSFER_DONE:
                break;
            case SCH_I2C_EVENT_TRANSFER_ERROR:
                break;
            default:
                break;
        }
    }
}
