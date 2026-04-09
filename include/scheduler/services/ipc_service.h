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

bool sch_ipc_service_init(
    sch_ipc_service_t *service,
    const sch_ipc_hal_t *hal,
    sch_ipc_irq_record_t *record_storage,
    size_t record_capacity,
    size_t max_records_per_run,
    sch_overflow_mode_t overflow_mode);

void sch_ipc_isr_notify(
    sch_ipc_service_t *service, uint16_t endpoint, uint16_t slot, uint16_t payload_flags);

void sch_ipc_isr_ack(sch_ipc_service_t *service, uint16_t endpoint, uint16_t slot, uint16_t ack_flags);

void sch_ipc_isr_fault(
    sch_ipc_service_t *service, uint16_t endpoint, uint16_t fault_flags, uint16_t source_slot);

void sch_ipc_service_run(void *ctx);

size_t sch_ipc_service_drop_count(const sch_ipc_service_t *service);

#ifdef __cplusplus
}
#endif

#endif
