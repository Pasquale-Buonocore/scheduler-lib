/**
 * @file ethernet_service.h
 * @brief Ethernet ISR/event-queue service API.
 */

#ifndef SCHEDULER_SERVICES_ETHERNET_SERVICE_H_
#define SCHEDULER_SERVICES_ETHERNET_SERVICE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "scheduler/isr_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Ethernet event identifiers queued from ISR to service task. */
typedef enum {
    SCH_ETH_EVENT_RX_READY = 1,
    SCH_ETH_EVENT_TX_DONE = 2,
    SCH_ETH_EVENT_LINK_CHANGE = 3
} sch_eth_event_id_t;

/** @brief HAL extension points for Ethernet descriptor/link processing. */
typedef struct {
    void (*ack_irq)(void *hal_ctx);
    bool (*fetch_rx_descriptor)(void *hal_ctx, uint16_t *out_desc);
    bool (*complete_tx_descriptor)(void *hal_ctx, uint16_t *out_desc);
    bool (*service_link_state)(void *hal_ctx);
    void *hal_ctx;
} sch_eth_hal_t;

/** @brief Ethernet service state container. */
typedef struct {
    sch_eth_hal_t hal;
    sch_event_queue_t event_queue;
    /** ISR/task wake hint; set/read/clear must be inside sch_port critical sections. */
    volatile bool irq_hint;
    size_t max_events_per_run;
} sch_eth_service_t;

/**
 * @brief Initialize Ethernet service and event queue.
 *
 * Copies HAL callbacks/context, clears wake hints, and initializes the
 * ISR-produced event queue used by @ref sch_eth_service_run.
 *
 * @param service Service instance to initialize.
 * @param hal Ethernet HAL callback table and opaque HAL context.
 * @param event_storage Caller-provided queue storage for event IDs.
 * @param event_capacity Number of event IDs available in @p event_storage.
 * @param max_events_per_run Upper bound on events drained per service run.
 *
 * @retval true Initialization succeeded.
 * @retval false Invalid arguments or queue initialization failed.
 */
bool sch_eth_service_init(
    sch_eth_service_t *service,
    const sch_eth_hal_t *hal,
    uint16_t *event_storage,
    size_t event_capacity,
    size_t max_events_per_run);

/**
 * @brief Ethernet RX ISR hook.
 *
 * Acknowledges the IRQ through HAL, queues @ref SCH_ETH_EVENT_RX_READY, and
 * sets the service wake hint for deferred task-side processing.
 *
 * @param service Service instance.
 */
void sch_eth_isr_rx(sch_eth_service_t *service);
/**
 * @brief Ethernet TX ISR hook.
 *
 * Acknowledges the IRQ through HAL, queues @ref SCH_ETH_EVENT_TX_DONE, and
 * sets the service wake hint for deferred task-side processing.
 *
 * @param service Service instance.
 */
void sch_eth_isr_tx(sch_eth_service_t *service);
/**
 * @brief Ethernet link-change ISR hook.
 *
 * Acknowledges the IRQ through HAL, queues @ref SCH_ETH_EVENT_LINK_CHANGE, and
 * sets the service wake hint for deferred task-side processing.
 *
 * @param service Service instance.
 */
void sch_eth_isr_link(sch_eth_service_t *service);

/**
 * @brief Execute one bounded Ethernet service cycle.
 *
 * Drains queued events up to @ref sch_eth_service_t.max_events_per_run and
 * dispatches each event to the relevant HAL callback.
 *
 * @param ctx Pointer to @ref sch_eth_service_t.
 */
void sch_eth_service_run(void *ctx);

#ifdef __cplusplus
}
#endif /* SCHEDULER_SERVICES_ETHERNET_SERVICE_H_ */

#endif
