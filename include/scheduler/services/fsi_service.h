/**
 * @file fsi_service.h
 * @brief FSI ISR record handoff service API.
 */

#ifndef SCHEDULER_SERVICES_FSI_SERVICE_H_
#define SCHEDULER_SERVICES_FSI_SERVICE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "scheduler/isr_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Advisory scheduler event bits for FSI service wakeup hints. */
typedef enum {
    SCH_FSI_EVENT_BIT_RX = (1u << 0),
    SCH_FSI_EVENT_BIT_TX = (1u << 1),
    SCH_FSI_EVENT_BIT_ERROR = (1u << 2)
} sch_fsi_event_bit_t;

/** @brief Minimal ISR-captured metadata handed from ISR to service task. */
typedef struct {
    uint16_t channel;
    uint16_t frame_index;
    uint16_t irq_tag;
    uint16_t status;
} sch_fsi_irq_record_t;

/** @brief Logical FSI IRQ classes used by @ref sch_fsi_irq_record_t.irq_tag. */
typedef enum {
    SCH_FSI_IRQ_TAG_RX = 1,
    SCH_FSI_IRQ_TAG_TX = 2,
    SCH_FSI_IRQ_TAG_ERROR = 3
} sch_fsi_irq_tag_t;

/** @brief HAL extension points owned by target-specific FSI driver. */
typedef struct {
    void (*ack_irq)(void *hal_ctx, sch_fsi_irq_tag_t tag, uint16_t channel);
    bool (*service_rx_frame)(void *hal_ctx, const sch_fsi_irq_record_t *record);
    bool (*service_tx_complete)(void *hal_ctx, const sch_fsi_irq_record_t *record);
    bool (*service_error)(void *hal_ctx, const sch_fsi_irq_record_t *record);
    void *hal_ctx;
} sch_fsi_hal_t;

/** @brief FSI driver/service state container. */
typedef struct {
    sch_fsi_hal_t hal;
    sch_spsc_ring_t irq_records;
    /** ISR/task advisory bits; set/read/clear must be inside sch_port critical sections. */
    volatile uint32_t event_bits;
    size_t max_records_per_run;
} sch_fsi_service_t;

/**
 * @brief Initialize FSI service state and IRQ record ring.
 *
 * Copies HAL callbacks/context, resets advisory event bits, and initializes
 * IRQ record buffering for deferred task-side dispatch.
 *
 * @param service Service instance to initialize.
 * @param hal FSI HAL callback table and opaque HAL context.
 * @param record_storage Caller-provided storage for IRQ records.
 * @param record_capacity Number of IRQ records available in storage.
 * @param max_records_per_run Upper bound on records drained per run.
 * @param overflow_mode Overflow policy applied when IRQ record ring is full.
 *
 * @retval true Initialization succeeded.
 * @retval false Invalid arguments or ring initialization failed.
 */
bool sch_fsi_service_init(
    sch_fsi_service_t *service,
    const sch_fsi_hal_t *hal,
    sch_fsi_irq_record_t *record_storage,
    size_t record_capacity,
    size_t max_records_per_run,
    sch_overflow_mode_t overflow_mode);

/**
 * @brief FSI RX ISR hook.
 *
 * Captures RX metadata and sets @ref SCH_FSI_EVENT_BIT_RX.
 *
 * @param service Service instance.
 * @param channel FSI channel identifier.
 * @param frame_index Frame index associated with RX completion.
 * @param status HAL-defined RX status bits.
 */
void sch_fsi_isr_rx(
    sch_fsi_service_t *service, uint16_t channel, uint16_t frame_index, uint16_t status);

/**
 * @brief FSI TX ISR hook.
 *
 * Captures TX metadata and sets @ref SCH_FSI_EVENT_BIT_TX.
 *
 * @param service Service instance.
 * @param channel FSI channel identifier.
 * @param frame_index Frame index associated with TX completion.
 * @param status HAL-defined TX status bits.
 */
void sch_fsi_isr_tx(
    sch_fsi_service_t *service, uint16_t channel, uint16_t frame_index, uint16_t status);

/**
 * @brief FSI error ISR hook.
 *
 * Captures error metadata and sets @ref SCH_FSI_EVENT_BIT_ERROR.
 *
 * @param service Service instance.
 * @param channel FSI channel identifier.
 * @param error_status HAL-defined error status bits.
 */
void sch_fsi_isr_error(sch_fsi_service_t *service, uint16_t channel, uint16_t error_status);

/**
 * @brief Execute one bounded FSI service cycle.
 *
 * Clears pending event bits, drains captured IRQ records, and dispatches each
 * record to the HAL callback matching its IRQ tag.
 *
 * @param ctx Pointer to @ref sch_fsi_service_t.
 */
void sch_fsi_service_run(void *ctx);

/**
 * @brief Read count of dropped FSI IRQ records.
 *
 * @param service Service instance.
 *
 * @return Total number of records dropped due to ring overflow.
 */
size_t sch_fsi_service_drop_count(const sch_fsi_service_t *service);

#ifdef __cplusplus
}
#endif /* SCHEDULER_SERVICES_FSI_SERVICE_H_ */

#endif
