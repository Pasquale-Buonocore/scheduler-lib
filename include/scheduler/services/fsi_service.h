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
    uint8_t channel;
    uint8_t frame_index;
    uint8_t irq_tag;
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
    void (*ack_irq)(void *hal_ctx, sch_fsi_irq_tag_t tag, uint8_t channel);
    bool (*service_rx_frame)(void *hal_ctx, const sch_fsi_irq_record_t *record);
    bool (*service_tx_complete)(void *hal_ctx, const sch_fsi_irq_record_t *record);
    bool (*service_error)(void *hal_ctx, const sch_fsi_irq_record_t *record);
    void *hal_ctx;
} sch_fsi_hal_t;

/** @brief FSI driver/service state container. */
typedef struct {
    sch_fsi_hal_t hal;
    sch_spsc_ring_t irq_records;
    volatile uint32_t event_bits;
    size_t max_records_per_run;
} sch_fsi_service_t;

bool sch_fsi_service_init(
    sch_fsi_service_t *service,
    const sch_fsi_hal_t *hal,
    sch_fsi_irq_record_t *record_storage,
    size_t record_capacity,
    size_t max_records_per_run,
    sch_overflow_mode_t overflow_mode);

void sch_fsi_isr_rx(
    sch_fsi_service_t *service, uint8_t channel, uint8_t frame_index, uint16_t status);

void sch_fsi_isr_tx(
    sch_fsi_service_t *service, uint8_t channel, uint8_t frame_index, uint16_t status);

void sch_fsi_isr_error(sch_fsi_service_t *service, uint8_t channel, uint16_t error_status);

void sch_fsi_service_run(void *ctx);

size_t sch_fsi_service_drop_count(const sch_fsi_service_t *service);

#ifdef __cplusplus
}
#endif

#endif
