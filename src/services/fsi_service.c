#include "scheduler/services/fsi_service.h"
#include "scheduler/port/scheduler_port.h"

static void sch_fsi_isr_push(
    sch_fsi_service_t *service,
    sch_fsi_irq_tag_t tag,
    uint16_t channel,
    uint16_t frame_index,
    uint16_t status,
    uint32_t event_bit) {
    if (service == NULL) {
        return;
    }

    if (service->hal.ack_irq != NULL) {
        service->hal.ack_irq(service->hal.hal_ctx, tag, channel);
    }

    sch_fsi_irq_record_t record = {
        .channel = channel,
        .frame_index = frame_index,
        .irq_tag = (uint16_t)tag,
        .status = status,
    };
    (void)sch_spsc_ring_push_isr(&service->irq_records, &record);
    uint32_t state = sch_port_enter_critical();
    service->event_bits |= event_bit;
    sch_port_exit_critical(state);
}

bool sch_fsi_service_init(
    sch_fsi_service_t *service,
    const sch_fsi_hal_t *hal,
    sch_fsi_irq_record_t *record_storage,
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
        sizeof(sch_fsi_irq_record_t),
        overflow_mode);
}

void sch_fsi_isr_rx(
    sch_fsi_service_t *service, uint16_t channel, uint16_t frame_index, uint16_t status) {
    sch_fsi_isr_push(
        service, SCH_FSI_IRQ_TAG_RX, channel, frame_index, status, SCH_FSI_EVENT_BIT_RX);
}

void sch_fsi_isr_tx(
    sch_fsi_service_t *service, uint16_t channel, uint16_t frame_index, uint16_t status) {
    sch_fsi_isr_push(
        service, SCH_FSI_IRQ_TAG_TX, channel, frame_index, status, SCH_FSI_EVENT_BIT_TX);
}

void sch_fsi_isr_error(sch_fsi_service_t *service, uint16_t channel, uint16_t error_status) {
    sch_fsi_isr_push(
        service,
        SCH_FSI_IRQ_TAG_ERROR,
        channel,
        0u,
        error_status,
        SCH_FSI_EVENT_BIT_ERROR);
}

void sch_fsi_service_run(void *ctx) {
    sch_fsi_service_t *service = (sch_fsi_service_t *)ctx;
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

    sch_fsi_irq_record_t record = {0};
    for (size_t i = 0u; i < service->max_records_per_run; ++i) {
        if (!sch_spsc_ring_pop_task(&service->irq_records, &record)) {
            break;
        }

        switch ((sch_fsi_irq_tag_t)record.irq_tag) {
            case SCH_FSI_IRQ_TAG_RX:
                if (service->hal.service_rx_frame != NULL) {
                    (void)service->hal.service_rx_frame(service->hal.hal_ctx, &record);
                }
                break;
            case SCH_FSI_IRQ_TAG_TX:
                if (service->hal.service_tx_complete != NULL) {
                    (void)service->hal.service_tx_complete(service->hal.hal_ctx, &record);
                }
                break;
            case SCH_FSI_IRQ_TAG_ERROR:
                if (service->hal.service_error != NULL) {
                    (void)service->hal.service_error(service->hal.hal_ctx, &record);
                }
                break;
            default:
                break;
        }
    }
}

size_t sch_fsi_service_drop_count(const sch_fsi_service_t *service) {
    if (service == NULL) {
        return 0u;
    }

    return sch_spsc_ring_drop_count(&service->irq_records);
}
