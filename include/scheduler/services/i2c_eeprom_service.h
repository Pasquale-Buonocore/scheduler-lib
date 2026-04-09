#ifndef SCHEDULER_SERVICES_I2C_EEPROM_SERVICE_H_
#define SCHEDULER_SERVICES_I2C_EEPROM_SERVICE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "scheduler/isr_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SCH_I2C_EVENT_TRANSFER_DONE = 1,
    SCH_I2C_EVENT_TRANSFER_ERROR = 2
} sch_i2c_event_id_t;

typedef struct {
    uint16_t address;
    uint16_t length;
    bool write;
} sch_eeprom_request_t;

typedef struct {
    void (*ack_irq)(void *hal_ctx);
    bool (*submit_request)(void *hal_ctx, const sch_eeprom_request_t *request);
    bool (*consume_completion)(void *hal_ctx, sch_i2c_event_id_t *out_event);
    void *hal_ctx;
} sch_i2c_eeprom_hal_t;

typedef struct {
    sch_i2c_eeprom_hal_t hal;
    sch_spsc_ring_t request_ring;
    sch_event_queue_t completion_queue;
    /** ISR/task wake hint; set/read/clear must be inside sch_port critical sections. */
    volatile bool irq_hint;
    size_t max_requests_per_run;
    size_t max_completions_per_run;
} sch_i2c_eeprom_service_t;

bool sch_i2c_eeprom_service_init(
    sch_i2c_eeprom_service_t *service,
    const sch_i2c_eeprom_hal_t *hal,
    sch_eeprom_request_t *request_storage,
    size_t request_capacity,
    uint16_t *completion_storage,
    size_t completion_capacity,
    size_t max_requests_per_run,
    size_t max_completions_per_run);

bool sch_i2c_eeprom_enqueue_request_isr(
    sch_i2c_eeprom_service_t *service,
    const sch_eeprom_request_t *request);

void sch_i2c_eeprom_isr_complete(sch_i2c_eeprom_service_t *service);

void sch_i2c_eeprom_service_run(void *ctx);

#ifdef __cplusplus
}
#endif

#endif
