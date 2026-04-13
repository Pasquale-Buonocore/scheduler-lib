/**
 * @file uart_service.h
 * @brief UART ISR handoff service API.
 */

#ifndef SCHEDULER_SERVICES_UART_SERVICE_H_
#define SCHEDULER_SERVICES_UART_SERVICE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "scheduler/isr_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief HAL extension points for platform UART integration. */
typedef struct {
    /** @brief Try reading one byte from UART RX hardware/FIFO. */
    bool (*try_read_byte)(void *hal_ctx, uint16_t *out_byte);
    /** @brief Try writing one byte to UART TX hardware/FIFO. */
    bool (*try_write_byte)(void *hal_ctx, uint16_t byte);
    /** @brief Optional interrupt acknowledge hook. */
    void (*ack_irq)(void *hal_ctx);
    /** @brief Opaque HAL context pointer passed to callbacks. */
    void *hal_ctx;
} sch_uart_hal_t;

/** @brief UART service state for ISR-producer/task-consumer buffering. */
typedef struct {
    sch_uart_hal_t hal;   /**< Bound UART HAL callbacks/context. */
    sch_spsc_ring_t rx_ring; /**< RX byte ring populated from ISR. */
    sch_spsc_ring_t tx_ring; /**< TX byte ring drained by service task. */
    /** ISR/task wake hints; reads/writes are guarded by sch_port critical sections. */
    volatile bool rx_hint;
    volatile bool tx_hint;
    size_t max_rx_items_per_run; /**< RX drain budget per @ref sch_uart_service_run call. */
    size_t max_tx_items_per_run; /**< TX drain budget per @ref sch_uart_service_run call. */
} sch_uart_service_t;

/**
 * @brief Initialize UART service state and ISR handoff rings.
 *
 * @param service Service instance to initialize.
 * @param hal HAL callback table and context.
 * @param rx_storage Caller-provided storage for RX ring elements.
 * @param rx_capacity RX ring capacity in elements.
 * @param tx_storage Caller-provided storage for TX ring elements.
 * @param tx_capacity TX ring capacity in elements.
 * @param max_rx_items_per_run Maximum RX items processed per run.
 * @param max_tx_items_per_run Maximum TX items processed per run.
 *
 * @retval true Service initialized successfully.
 * @retval false Invalid arguments or ring initialization failure.
 */
bool sch_uart_service_init(
    sch_uart_service_t *service,
    const sch_uart_hal_t *hal,
    uint16_t *rx_storage,
    size_t rx_capacity,
    uint16_t *tx_storage,
    size_t tx_capacity,
    size_t max_rx_items_per_run,
    size_t max_tx_items_per_run);

/**
 * @brief UART RX ISR entrypoint.
 *
 * Captures one received byte, enqueues it, and sets an RX wake hint.
 *
 * @param service Service instance.
 */
void sch_uart_isr_rx(sch_uart_service_t *service);

/**
 * @brief UART TX-ready ISR entrypoint.
 *
 * Sets a TX wake hint so task context can attempt queued TX writes.
 *
 * @param service Service instance.
 */
void sch_uart_isr_tx_ready(sch_uart_service_t *service);

/**
 * @brief Queue a byte for deferred UART TX from service context.
 *
 * @param service Service instance.
 * @param byte Byte value to enqueue.
 *
 * @retval true Byte was enqueued.
 * @retval false Service was NULL or TX ring was full.
 */
bool sch_uart_service_queue_tx(sch_uart_service_t *service, uint16_t byte);

/**
 * @brief Drain UART RX/TX queues in bounded task context.
 *
 * @param ctx Pointer to @ref sch_uart_service_t.
 */
void sch_uart_service_run(void *ctx);

#ifdef __cplusplus
}
#endif /* SCHEDULER_SERVICES_UART_SERVICE_H_ */

#endif
