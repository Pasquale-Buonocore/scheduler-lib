#include "scheduler/services/ipc_service.h"
#include "scheduler/port/scheduler_port.h"

/**
 * @brief Common ISR helper to capture IPC metadata and set event bit.
 *
 * @param service Service instance.
 * @param tag IRQ class tag.
 * @param endpoint Endpoint associated with interrupt.
 * @param slot Slot/source metadata.
 * @param detail_flags HAL-defined detail bits.
 * @param event_bit Advisory bit to OR into pending event set.
 */
static void sch_ipc_isr_push(
    sch_ipc_service_t *service,
    sch_ipc_irq_tag_t tag,
    uint16_t endpoint,
    uint16_t slot,
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
        .irq_tag = (uint16_t)tag,
        .detail_flags = detail_flags,
    };

    (void)sch_spsc_ring_push_isr(&service->irq_records, &record);
    uint32_t state = sch_port_enter_critical();
    service->event_bits |= event_bit;
    sch_port_exit_critical(state);
}

/**
 * @brief Initialize IPC service ring, budgets, and event bits.
 *
 * Mirrors @ref sch_ipc_service_init contract from the public header.
 */
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

/**
 * @brief Capture IPC notify IRQ record.
 *
 * @param service Service instance.
 * @param endpoint Endpoint identifier.
 * @param slot Slot identifier.
 * @param payload_flags HAL-defined payload detail flags.
 */
void sch_ipc_isr_notify(
    sch_ipc_service_t *service, uint16_t endpoint, uint16_t slot, uint16_t payload_flags) {
    sch_ipc_isr_push(
        service, SCH_IPC_IRQ_TAG_NOTIFY, endpoint, slot, payload_flags, SCH_IPC_EVENT_BIT_NOTIFY);
}

/**
 * @brief Capture IPC ack IRQ record.
 *
 * @param service Service instance.
 * @param endpoint Endpoint identifier.
 * @param slot Slot identifier.
 * @param ack_flags HAL-defined ack detail flags.
 */
void sch_ipc_isr_ack(sch_ipc_service_t *service, uint16_t endpoint, uint16_t slot, uint16_t ack_flags) {
    sch_ipc_isr_push(service, SCH_IPC_IRQ_TAG_ACK, endpoint, slot, ack_flags, SCH_IPC_EVENT_BIT_ACK);
}

/**
 * @brief Capture IPC fault IRQ record.
 *
 * @param service Service instance.
 * @param endpoint Endpoint identifier.
 * @param fault_flags HAL-defined fault detail flags.
 * @param source_slot Slot/source identifier.
 */
void sch_ipc_isr_fault(
    sch_ipc_service_t *service, uint16_t endpoint, uint16_t fault_flags, uint16_t source_slot) {
    sch_ipc_isr_push(
        service,
        SCH_IPC_IRQ_TAG_FAULT,
        endpoint,
        source_slot,
        fault_flags,
        SCH_IPC_EVENT_BIT_FAULT);
}

/**
 * @brief Execute one bounded IPC service cycle.
 *
 * @param ctx Pointer to @ref sch_ipc_service_t.
 */
void sch_ipc_service_run(void *ctx) {
    sch_ipc_service_t *service = (sch_ipc_service_t *)ctx;
    if (service == NULL) {
        return;
    }

    uint32_t pending_event_bits = 0u;
    uint32_t state = sch_port_enter_critical();
    pending_event_bits = service->event_bits;
    service->event_bits = 0u;
    sch_port_exit_critical(state);

    if ((pending_event_bits == 0u) && sch_spsc_ring_is_empty(&service->irq_records)) {
        return;
    }

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

/**
 * @brief Return accumulated drop count for IRQ record ring.
 *
 * @param service Service instance.
 *
 * @return Drop counter value.
 */
size_t sch_ipc_service_drop_count(const sch_ipc_service_t *service) {
    if (service == NULL) {
        return 0u;
    }

    return sch_spsc_ring_drop_count(&service->irq_records);
}
