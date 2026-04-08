#include "scheduler/services/ipc_service.h"

static void sch_ipc_isr_push(
    sch_ipc_service_t *service,
    sch_ipc_irq_tag_t tag,
    uint8_t endpoint,
    uint8_t slot,
    uint16_t detail_flags,
    uint32_t event_bit) {
    if (service == NULL) {
        return;
    }

    if (service->hal.ack_irq != NULL) {
        service->hal.ack_irq(service->hal.hal_ctx, tag, endpoint);
    }

    sch_ipc_irq_record_t record = {
        .endpoint = endpoint,
        .slot = slot,
        .irq_tag = (uint8_t)tag,
        .detail_flags = detail_flags,
    };

    (void)sch_spsc_ring_push_isr(&service->irq_records, &record);
    service->event_bits |= event_bit;
}

bool sch_ipc_service_init(
    sch_ipc_service_t *service,
    const sch_ipc_hal_t *hal,
    sch_ipc_irq_record_t *record_storage,
    size_t record_capacity,
    size_t max_records_per_run,
    sch_overflow_mode_t overflow_mode) {
    if ((service == NULL) || (hal == NULL) || (max_records_per_run == 0u)) {
        return false;
    }

    service->hal = *hal;
    service->event_bits = 0u;
    service->max_records_per_run = max_records_per_run;

    return sch_spsc_ring_init(
        &service->irq_records,
        record_storage,
        record_capacity,
        sizeof(sch_ipc_irq_record_t),
        overflow_mode);
}

void sch_ipc_isr_notify(
    sch_ipc_service_t *service, uint8_t endpoint, uint8_t slot, uint16_t payload_flags) {
    sch_ipc_isr_push(
        service, SCH_IPC_IRQ_TAG_NOTIFY, endpoint, slot, payload_flags, SCH_IPC_EVENT_BIT_NOTIFY);
}

void sch_ipc_isr_ack(sch_ipc_service_t *service, uint8_t endpoint, uint8_t slot, uint16_t ack_flags) {
    sch_ipc_isr_push(service, SCH_IPC_IRQ_TAG_ACK, endpoint, slot, ack_flags, SCH_IPC_EVENT_BIT_ACK);
}

void sch_ipc_isr_fault(
    sch_ipc_service_t *service, uint8_t endpoint, uint16_t fault_flags, uint8_t source_slot) {
    sch_ipc_isr_push(
        service,
        SCH_IPC_IRQ_TAG_FAULT,
        endpoint,
        source_slot,
        fault_flags,
        SCH_IPC_EVENT_BIT_FAULT);
}

void sch_ipc_service_run(void *ctx) {
    sch_ipc_service_t *service = (sch_ipc_service_t *)ctx;
    if (service == NULL) {
        return;
    }

    if ((service->event_bits == 0u) && sch_spsc_ring_is_empty(&service->irq_records)) {
        return;
    }

    service->event_bits = 0u;

    sch_ipc_irq_record_t record = {0};
    for (size_t i = 0u; i < service->max_records_per_run; ++i) {
        if (!sch_spsc_ring_pop_task(&service->irq_records, &record)) {
            break;
        }

        switch ((sch_ipc_irq_tag_t)record.irq_tag) {
            case SCH_IPC_IRQ_TAG_NOTIFY:
                if (service->hal.service_notify != NULL) {
                    (void)service->hal.service_notify(service->hal.hal_ctx, &record);
                }
                break;
            case SCH_IPC_IRQ_TAG_ACK:
                if (service->hal.service_ack != NULL) {
                    (void)service->hal.service_ack(service->hal.hal_ctx, &record);
                }
                break;
            case SCH_IPC_IRQ_TAG_FAULT:
                if (service->hal.service_fault != NULL) {
                    (void)service->hal.service_fault(service->hal.hal_ctx, &record);
                }
                break;
            default:
                break;
        }
    }
}

size_t sch_ipc_service_drop_count(const sch_ipc_service_t *service) {
    if (service == NULL) {
        return 0u;
    }

    return sch_spsc_ring_drop_count(&service->irq_records);
}
