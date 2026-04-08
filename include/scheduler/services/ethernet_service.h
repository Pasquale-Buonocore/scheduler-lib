#ifndef SCHEDULER_SERVICES_ETHERNET_SERVICE_H_
#define SCHEDULER_SERVICES_ETHERNET_SERVICE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "scheduler/isr_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SCH_ETH_EVENT_RX_READY = 1,
    SCH_ETH_EVENT_TX_DONE = 2,
    SCH_ETH_EVENT_LINK_CHANGE = 3
} sch_eth_event_id_t;

typedef struct {
    void (*ack_irq)(void *hal_ctx);
    bool (*fetch_rx_descriptor)(void *hal_ctx, uint16_t *out_desc);
    bool (*complete_tx_descriptor)(void *hal_ctx, uint16_t *out_desc);
    bool (*service_link_state)(void *hal_ctx);
    void *hal_ctx;
} sch_eth_hal_t;

typedef struct {
    sch_eth_hal_t hal;
    sch_event_queue_t event_queue;
    volatile bool irq_hint;
    size_t max_events_per_run;
} sch_eth_service_t;

bool sch_eth_service_init(
    sch_eth_service_t *service,
    const sch_eth_hal_t *hal,
    uint16_t *event_storage,
    size_t event_capacity,
    size_t max_events_per_run);

void sch_eth_isr_rx(sch_eth_service_t *service);
void sch_eth_isr_tx(sch_eth_service_t *service);
void sch_eth_isr_link(sch_eth_service_t *service);

void sch_eth_service_run(void *ctx);

#ifdef __cplusplus
}
#endif

#endif
