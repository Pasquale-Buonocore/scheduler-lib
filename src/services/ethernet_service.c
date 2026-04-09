#include "scheduler/services/ethernet_service.h"
#include "scheduler/port/scheduler_port.h"

bool sch_eth_service_init(
    sch_eth_service_t *service,
    const sch_eth_hal_t *hal,
    uint16_t *event_storage,
    size_t event_capacity,
    size_t max_events_per_run) {
    if ((service == NULL) || (hal == NULL) || (hal->ack_irq == NULL) ||
        (max_events_per_run == 0u)) {
        return false;
    }

    service->hal = *hal;
    service->irq_hint = false;
    service->max_events_per_run = max_events_per_run;

    return sch_event_queue_init(
        &service->event_queue, event_storage, event_capacity, SCH_OVERFLOW_DROP_OLDEST);
}

static void sch_eth_push_event_isr(sch_eth_service_t *service, sch_eth_event_id_t event_id) {
    if (service == NULL) {
        return;
    }

    service->hal.ack_irq(service->hal.hal_ctx);
    (void)sch_event_queue_push_isr(&service->event_queue, (uint16_t)event_id);
    uint32_t state = sch_port_enter_critical();
    service->irq_hint = true;
    sch_port_exit_critical(state);
}

void sch_eth_isr_rx(sch_eth_service_t *service) {
    sch_eth_push_event_isr(service, SCH_ETH_EVENT_RX_READY);
}

void sch_eth_isr_tx(sch_eth_service_t *service) {
    sch_eth_push_event_isr(service, SCH_ETH_EVENT_TX_DONE);
}

void sch_eth_isr_link(sch_eth_service_t *service) {
    sch_eth_push_event_isr(service, SCH_ETH_EVENT_LINK_CHANGE);
}

void sch_eth_service_run(void *ctx) {
    sch_eth_service_t *service = (sch_eth_service_t *)ctx;
    if (service == NULL) {
        return;
    }

    bool had_irq_hint = false;
    uint32_t state = sch_port_enter_critical();
    had_irq_hint = service->irq_hint;
    service->irq_hint = false;
    sch_port_exit_critical(state);

    if (!had_irq_hint && (sch_event_queue_size(&service->event_queue) == 0u)) {
        return;
    }

    uint16_t event_id = 0u;
    for (size_t i = 0u; i < service->max_events_per_run; ++i) {
        if (!sch_event_queue_pop_task(&service->event_queue, &event_id)) {
            break;
        }

        switch ((sch_eth_event_id_t)event_id) {
            case SCH_ETH_EVENT_RX_READY: {
                uint16_t rx_desc = 0u;
                if (service->hal.fetch_rx_descriptor != NULL) {
                    (void)service->hal.fetch_rx_descriptor(service->hal.hal_ctx, &rx_desc);
                }
                break;
            }
            case SCH_ETH_EVENT_TX_DONE: {
                uint16_t tx_desc = 0u;
                if (service->hal.complete_tx_descriptor != NULL) {
                    (void)service->hal.complete_tx_descriptor(service->hal.hal_ctx, &tx_desc);
                }
                break;
            }
            case SCH_ETH_EVENT_LINK_CHANGE:
                if (service->hal.service_link_state != NULL) {
                    (void)service->hal.service_link_state(service->hal.hal_ctx);
                }
                break;
            default:
                break;
        }
    }
}
