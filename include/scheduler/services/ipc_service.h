/**
 * @file ipc_service.h
 * @brief IPC ISR record handoff service API.
 */

#ifndef SCHEDULER_SERVICES_IPC_SERVICE_H_
#define SCHEDULER_SERVICES_IPC_SERVICE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "scheduler/isr_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Advisory scheduler event bits for IPC work classes. */
typedef enum {
    SCH_IPC_EVENT_BIT_NOTIFY = (1u << 0),
    SCH_IPC_EVENT_BIT_ACK = (1u << 1),
    SCH_IPC_EVENT_BIT_FAULT = (1u << 2)
} sch_ipc_event_bit_t;

/** @brief Minimal ISR-captured IPC metadata for deferred handling. */
typedef struct {
    uint16_t endpoint;
    uint16_t slot;
    uint16_t irq_tag;
    uint16_t detail_flags;
} sch_ipc_irq_record_t;

/** @brief Logical IRQ tags used in @ref sch_ipc_irq_record_t.irq_tag. */
typedef enum {
    SCH_IPC_IRQ_TAG_NOTIFY = 1,
    SCH_IPC_IRQ_TAG_ACK = 2,
    SCH_IPC_IRQ_TAG_FAULT = 3
} sch_ipc_irq_tag_t;

/** @brief HAL extension points for platform-specific IPC interactions. */
typedef struct {
    void (*ack_irq)(void *hal_ctx, sch_ipc_irq_tag_t tag, uint16_t endpoint);
    bool (*service_notify)(void *hal_ctx, const sch_ipc_irq_record_t *record);
    bool (*service_ack)(void *hal_ctx, const sch_ipc_irq_record_t *record);
    bool (*service_fault)(void *hal_ctx, const sch_ipc_irq_record_t *record);
    void *hal_ctx;
} sch_ipc_hal_t;

/** @brief IPC interrupt handoff/service state. */
typedef struct {
    sch_ipc_hal_t hal;
    sch_spsc_ring_t irq_records;
    /** ISR/task advisory bits; set/read/clear must be inside sch_port critical sections. */
    volatile uint32_t event_bits;
    size_t max_records_per_run;
} sch_ipc_service_t;

/**
 * @brief Initialize IPC service state and IRQ record ring.
 *
 * Copies HAL callbacks/context, resets advisory event bits, and initializes
 * IRQ record buffering for deferred task-side dispatch.
 *
 * @param service Service instance to initialize.
 * @param hal IPC HAL callback table and opaque HAL context.
 * @param record_storage Caller-provided storage for IRQ records.
 * @param record_capacity Number of IRQ records available in storage.
 * @param max_records_per_run Upper bound on records drained per run.
 * @param overflow_mode Overflow policy applied when IRQ record ring is full.
 *
 * @retval true Initialization succeeded.
 * @retval false Invalid arguments or ring initialization failed.
 */
bool sch_ipc_service_init(
    sch_ipc_service_t *service,
    const sch_ipc_hal_t *hal,
    sch_ipc_irq_record_t *record_storage,
    size_t record_capacity,
    size_t max_records_per_run,
    sch_overflow_mode_t overflow_mode);

/**
 * @brief IPC notify ISR hook.
 *
 * Captures notify metadata and sets @ref SCH_IPC_EVENT_BIT_NOTIFY.
 *
 * @param service Service instance.
 * @param endpoint Source/destination endpoint identifier.
 * @param slot Mailbox or slot index associated with the IRQ.
 * @param payload_flags HAL-defined payload detail flags.
 */
void sch_ipc_isr_notify(
    sch_ipc_service_t *service, uint16_t endpoint, uint16_t slot, uint16_t payload_flags);

/**
 * @brief IPC ack ISR hook.
 *
 * Captures ack metadata and sets @ref SCH_IPC_EVENT_BIT_ACK.
 *
 * @param service Service instance.
 * @param endpoint Source/destination endpoint identifier.
 * @param slot Mailbox or slot index associated with the IRQ.
 * @param ack_flags HAL-defined acknowledgment detail flags.
 */
void sch_ipc_isr_ack(sch_ipc_service_t *service, uint16_t endpoint, uint16_t slot, uint16_t ack_flags);

/**
 * @brief IPC fault ISR hook.
 *
 * Captures fault metadata and sets @ref SCH_IPC_EVENT_BIT_FAULT.
 *
 * @param service Service instance.
 * @param endpoint Endpoint associated with the fault.
 * @param fault_flags HAL-defined fault detail flags.
 * @param source_slot Slot or source identifier associated with the fault.
 */
void sch_ipc_isr_fault(
    sch_ipc_service_t *service, uint16_t endpoint, uint16_t fault_flags, uint16_t source_slot);

/**
 * @brief Execute one bounded IPC service cycle.
 *
 * Clears pending event bits, drains captured IRQ records, and dispatches each
 * record to the HAL callback matching its IRQ tag.
 *
 * @param ctx Pointer to @ref sch_ipc_service_t.
 */
void sch_ipc_service_run(void *ctx);

/**
 * @brief Read count of dropped IRQ records.
 *
 * @param service Service instance.
 *
 * @return Total number of records dropped due to ring overflow.
 */
size_t sch_ipc_service_drop_count(const sch_ipc_service_t *service);

#ifdef __cplusplus
}
#endif /* SCHEDULER_SERVICES_IPC_SERVICE_H_ */

#endif
