/**
 * @file i2c_eeprom_service.h
 * @brief I2C EEPROM request/completion service API.
 */

#ifndef SCHEDULER_SERVICES_I2C_EEPROM_SERVICE_H_
#define SCHEDULER_SERVICES_I2C_EEPROM_SERVICE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "scheduler/isr_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Completion events emitted by EEPROM I2C transfer path. */
typedef enum {
    SCH_I2C_EVENT_TRANSFER_DONE = 1,
    SCH_I2C_EVENT_TRANSFER_ERROR = 2
} sch_i2c_event_id_t;

/** @brief Fixed-size EEPROM transfer request descriptor. */
typedef struct {
    uint16_t address;
    uint16_t length;
    bool write;
} sch_eeprom_request_t;

/** @brief HAL extension points for platform EEPROM transfer handling. */
typedef struct {
    void (*ack_irq)(void *hal_ctx);
    bool (*submit_request)(void *hal_ctx, const sch_eeprom_request_t *request);
    bool (*consume_completion)(void *hal_ctx, sch_i2c_event_id_t *out_event);
    void *hal_ctx;
} sch_i2c_eeprom_hal_t;

/** @brief I2C EEPROM service state and ISR/task handoff queues. */
typedef struct {
    sch_i2c_eeprom_hal_t hal;
    sch_spsc_ring_t request_ring;
    sch_event_queue_t completion_queue;
    /** ISR/task wake hint; set/read/clear must be inside sch_port critical sections. */
    volatile bool irq_hint;
    size_t max_requests_per_run;
    size_t max_completions_per_run;
} sch_i2c_eeprom_service_t;

/**
 * @brief Initialize EEPROM service request/completion buffers.
 *
 * Copies HAL callbacks/context, clears wake hints, and initializes both the
 * request ring and completion event queue.
 *
 * @param service Service instance to initialize.
 * @param hal EEPROM HAL callback table and opaque HAL context.
 * @param request_storage Caller-provided storage for request records.
 * @param request_capacity Number of request records available in storage.
 * @param completion_storage Caller-provided storage for completion event IDs.
 * @param completion_capacity Number of completion IDs available in storage.
 * @param max_requests_per_run Upper bound on request submissions per run.
 * @param max_completions_per_run Upper bound on completion events handled per run.
 *
 * @retval true Initialization succeeded.
 * @retval false Invalid arguments or buffer initialization failed.
 */
bool sch_i2c_eeprom_service_init(
    sch_i2c_eeprom_service_t *service,
    const sch_i2c_eeprom_hal_t *hal,
    sch_eeprom_request_t *request_storage,
    size_t request_capacity,
    uint16_t *completion_storage,
    size_t completion_capacity,
    size_t max_requests_per_run,
    size_t max_completions_per_run);

/**
 * @brief Enqueue an EEPROM request from ISR/signal context.
 *
 * @param service Service instance.
 * @param request Transfer request to enqueue.
 *
 * @retval true Request was queued.
 * @retval false Invalid arguments or request queue full.
 */
bool sch_i2c_eeprom_enqueue_request_isr(
    sch_i2c_eeprom_service_t *service,
    const sch_eeprom_request_t *request);

/**
 * @brief Completion ISR hook; captures and queues completion event.
 *
 * The ISR optionally acknowledges hardware, reads completion status from HAL,
 * queues the resulting event ID, and sets the wake hint.
 *
 * @param service Service instance.
 */
void sch_i2c_eeprom_isr_complete(sch_i2c_eeprom_service_t *service);

/**
 * @brief Execute one bounded EEPROM service cycle.
 *
 * Drains queued requests and completion events with independent per-run
 * budgets to keep scheduler latency bounded.
 *
 * @param ctx Pointer to @ref sch_i2c_eeprom_service_t.
 */
void sch_i2c_eeprom_service_run(void *ctx);

#ifdef __cplusplus
}
#endif /* SCHEDULER_SERVICES_I2C_EEPROM_SERVICE_H_ */

#endif
