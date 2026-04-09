#ifndef SCHEDULER_SERVICES_UART_SERVICE_H_
#define SCHEDULER_SERVICES_UART_SERVICE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "scheduler/isr_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool (*try_read_byte)(void *hal_ctx, uint16_t *out_byte);
    bool (*try_write_byte)(void *hal_ctx, uint16_t byte);
    void (*ack_irq)(void *hal_ctx);
    void *hal_ctx;
} sch_uart_hal_t;

typedef struct {
    sch_uart_hal_t hal;
    sch_spsc_ring_t rx_ring;
    sch_spsc_ring_t tx_ring;
    /** ISR/task wake hints; reads/writes are guarded by sch_port critical sections. */
    volatile bool rx_hint;
    volatile bool tx_hint;
    size_t max_rx_items_per_run;
    size_t max_tx_items_per_run;
} sch_uart_service_t;

bool sch_uart_service_init(
    sch_uart_service_t *service,
    const sch_uart_hal_t *hal,
    uint16_t *rx_storage,
    size_t rx_capacity,
    uint16_t *tx_storage,
    size_t tx_capacity,
    size_t max_rx_items_per_run,
    size_t max_tx_items_per_run);

void sch_uart_isr_rx(sch_uart_service_t *service);

void sch_uart_isr_tx_ready(sch_uart_service_t *service);

bool sch_uart_service_queue_tx(sch_uart_service_t *service, uint16_t byte);

void sch_uart_service_run(void *ctx);

#ifdef __cplusplus
}
#endif

#endif
