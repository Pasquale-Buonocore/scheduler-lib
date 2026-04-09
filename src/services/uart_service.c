#include "scheduler/services/uart_service.h"
#include "scheduler/port/scheduler_port.h"

bool sch_uart_service_init(
    sch_uart_service_t *service,
    const sch_uart_hal_t *hal,
    uint16_t *rx_storage,
    size_t rx_capacity,
    uint16_t *tx_storage,
    size_t tx_capacity,
    size_t max_rx_items_per_run,
    size_t max_tx_items_per_run) {
    if ((service == NULL) || (hal == NULL) || (hal->try_read_byte == NULL) ||
        (hal->try_write_byte == NULL) || (max_rx_items_per_run == 0u) ||
        (max_tx_items_per_run == 0u)) {
        return false;
    }

    service->hal = *hal;
    service->rx_hint = false;
    service->tx_hint = false;
    service->max_rx_items_per_run = max_rx_items_per_run;
    service->max_tx_items_per_run = max_tx_items_per_run;

    bool ok_rx = sch_spsc_ring_init(
        &service->rx_ring, rx_storage, rx_capacity, sizeof(uint16_t), SCH_OVERFLOW_DROP_OLDEST);
    bool ok_tx = sch_spsc_ring_init(
        &service->tx_ring, tx_storage, tx_capacity, sizeof(uint16_t), SCH_OVERFLOW_DROP_NEWEST);

    return (ok_rx && ok_tx);
}

void sch_uart_isr_rx(sch_uart_service_t *service) {
    if (service == NULL) {
        return;
    }

    if (service->hal.ack_irq != NULL) {
        service->hal.ack_irq(service->hal.hal_ctx);
    }

    uint16_t byte = 0u;
    if (service->hal.try_read_byte(service->hal.hal_ctx, &byte)) {
        (void)sch_spsc_ring_push_isr(&service->rx_ring, &byte);
        uint32_t state = sch_port_enter_critical();
        service->rx_hint = true;
        sch_port_exit_critical(state);
    }
}

void sch_uart_isr_tx_ready(sch_uart_service_t *service) {
    if (service == NULL) {
        return;
    }

    if (service->hal.ack_irq != NULL) {
        service->hal.ack_irq(service->hal.hal_ctx);
    }

    uint32_t state = sch_port_enter_critical();
    service->tx_hint = true;
    sch_port_exit_critical(state);
}

bool sch_uart_service_queue_tx(sch_uart_service_t *service, uint16_t byte) {
    if (service == NULL) {
        return false;
    }

    return (sch_spsc_ring_push_isr(&service->tx_ring, &byte) == SCH_RING_PUSH_OK);
}

void sch_uart_service_run(void *ctx) {
    sch_uart_service_t *service = (sch_uart_service_t *)ctx;
    if (service == NULL) {
        return;
    }

    bool had_rx_hint = false;
    bool had_tx_hint = false;
    uint32_t state = sch_port_enter_critical();
    had_rx_hint = service->rx_hint;
    had_tx_hint = service->tx_hint;
    service->rx_hint = false;
    service->tx_hint = false;
    sch_port_exit_critical(state);

    if (!had_rx_hint && !had_tx_hint && sch_spsc_ring_is_empty(&service->rx_ring) &&
        sch_spsc_ring_is_empty(&service->tx_ring)) {
        return;
    }

    uint16_t byte = 0u;
    for (size_t i = 0u; i < service->max_tx_items_per_run; ++i) {
        if (!sch_spsc_ring_pop_task(&service->tx_ring, &byte)) {
            break;
        }

        if (!service->hal.try_write_byte(service->hal.hal_ctx, byte)) {
            (void)sch_spsc_ring_push_isr(&service->tx_ring, &byte);
            break;
        }
    }

    for (size_t i = 0u; i < service->max_rx_items_per_run; ++i) {
        if (!sch_spsc_ring_pop_task(&service->rx_ring, &byte)) {
            break;
        }

        /* Extension point: hand received bytes to parser/protocol layer. */
        (void)byte;
    }
}
