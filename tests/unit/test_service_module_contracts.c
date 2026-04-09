/**
 * @file test_service_module_contracts.c
 * @brief Contract-oriented unit tests for service skeleton modules.
 */

#include "unity.h"

#include "scheduler/isr_buffer.h"
#include "scheduler/services/ethernet_service.h"
#include "scheduler/services/fsi_service.h"
#include "scheduler/services/i2c_eeprom_service.h"
#include "scheduler/services/ipc_service.h"
#include "scheduler/services/motor_supervision_service.h"
#include "scheduler/services/uart_service.h"

static uint32_t critical_depth;

uint32_t sch_port_now_ticks(void) {
    return 0u;
}

uint32_t sch_port_enter_critical(void) {
    critical_depth++;
    return critical_depth;
}

void sch_port_exit_critical(uint32_t state) {
    (void)state;
    if (critical_depth > 0u) {
        critical_depth--;
    }
}

void sch_port_idle(void) {
}

/* ---------- Ethernet fakes ---------- */
static uint32_t eth_ack_count;
static uint32_t eth_rx_count;
static uint32_t eth_tx_count;
static uint32_t eth_link_count;

static void eth_ack_irq(void *ctx) {
    (void)ctx;
    eth_ack_count++;
}

static bool eth_fetch_rx_descriptor(void *ctx, uint16_t *out_desc) {
    (void)ctx;
    if (out_desc != NULL) {
        *out_desc = (uint16_t)eth_rx_count;
    }
    eth_rx_count++;
    return true;
}

static bool eth_complete_tx_descriptor(void *ctx, uint16_t *out_desc) {
    (void)ctx;
    if (out_desc != NULL) {
        *out_desc = (uint16_t)eth_tx_count;
    }
    eth_tx_count++;
    return true;
}

static bool eth_service_link_state(void *ctx) {
    (void)ctx;
    eth_link_count++;
    return true;
}

/* ---------- FSI fakes ---------- */
static uint32_t fsi_ack_count;
static uint32_t fsi_rx_count;
static uint32_t fsi_tx_count;
static uint32_t fsi_err_count;

static void fsi_ack_irq(void *ctx, sch_fsi_irq_tag_t tag, uint16_t channel) {
    (void)ctx;
    (void)tag;
    (void)channel;
    fsi_ack_count++;
}

static bool fsi_service_rx(void *ctx, const sch_fsi_irq_record_t *record) {
    (void)ctx;
    (void)record;
    fsi_rx_count++;
    return true;
}

static bool fsi_service_tx(void *ctx, const sch_fsi_irq_record_t *record) {
    (void)ctx;
    (void)record;
    fsi_tx_count++;
    return true;
}

static bool fsi_service_error(void *ctx, const sch_fsi_irq_record_t *record) {
    (void)ctx;
    (void)record;
    fsi_err_count++;
    return true;
}

/* ---------- UART fakes ---------- */
static uint32_t uart_ack_count;
static uint16_t uart_next_rx_byte;
static uint32_t uart_try_read_count;
static uint32_t uart_try_write_count;

static void uart_ack_irq(void *ctx) {
    (void)ctx;
    uart_ack_count++;
}

static bool uart_try_read(void *ctx, uint16_t *out_byte) {
    (void)ctx;
    if (out_byte != NULL) {
        *out_byte = uart_next_rx_byte;
    }
    uart_next_rx_byte++;
    uart_try_read_count++;
    return true;
}

static bool uart_try_write(void *ctx, uint16_t byte) {
    (void)ctx;
    (void)byte;
    uart_try_write_count++;
    return true;
}

/* ---------- I2C EEPROM fakes ---------- */
static uint32_t i2c_ack_count;
static uint32_t i2c_submit_count;
static uint32_t i2c_consume_count;
static sch_i2c_event_id_t i2c_next_completion_event;

static void i2c_ack_irq(void *ctx) {
    (void)ctx;
    i2c_ack_count++;
}

static bool i2c_submit_request(void *ctx, const sch_eeprom_request_t *request) {
    (void)ctx;
    (void)request;
    i2c_submit_count++;
    return true;
}

static bool i2c_consume_completion(void *ctx, sch_i2c_event_id_t *out_event) {
    (void)ctx;
    i2c_consume_count++;
    if (out_event != NULL) {
        *out_event = i2c_next_completion_event;
    }
    return true;
}

/* ---------- Motor fakes ---------- */
static uint32_t motor_ack_count;
static uint16_t motor_next_speed;
static uint32_t motor_read_feedback_count;
static uint32_t motor_set_enable_count;
static uint32_t motor_set_speed_count;

static void motor_ack_irq(void *ctx) {
    (void)ctx;
    motor_ack_count++;
}

static bool motor_read_feedback_sample(void *ctx, sch_motor_feedback_t *out_feedback) {
    (void)ctx;
    if (out_feedback != NULL) {
        out_feedback->speed_rpm = motor_next_speed;
        out_feedback->current_ma = 100;
    }
    motor_next_speed = (uint16_t)(motor_next_speed + 1u);
    motor_read_feedback_count++;
    return true;
}

static bool motor_set_enable(void *ctx, bool enable) {
    (void)ctx;
    (void)enable;
    motor_set_enable_count++;
    return true;
}

static bool motor_set_target_speed_rpm(void *ctx, uint16_t target_speed_rpm) {
    (void)ctx;
    (void)target_speed_rpm;
    motor_set_speed_count++;
    return true;
}

/* ---------- IPC fakes ---------- */
static uint32_t ipc_ack_count;
static uint32_t ipc_notify_count;
static uint32_t ipc_service_ack_count;
static uint32_t ipc_fault_count;

static void ipc_ack_irq(void *ctx, sch_ipc_irq_tag_t tag, uint16_t endpoint) {
    (void)ctx;
    (void)tag;
    (void)endpoint;
    ipc_ack_count++;
}

static bool ipc_service_notify(void *ctx, const sch_ipc_irq_record_t *record) {
    (void)ctx;
    (void)record;
    ipc_notify_count++;
    return true;
}

static bool ipc_service_ack(void *ctx, const sch_ipc_irq_record_t *record) {
    (void)ctx;
    (void)record;
    ipc_service_ack_count++;
    return true;
}

static bool ipc_service_fault(void *ctx, const sch_ipc_irq_record_t *record) {
    (void)ctx;
    (void)record;
    ipc_fault_count++;
    return true;
}

void setUp(void) {
    critical_depth = 0u;
    eth_ack_count = 0u;
    eth_rx_count = 0u;
    eth_tx_count = 0u;
    eth_link_count = 0u;
    fsi_ack_count = 0u;
    fsi_rx_count = 0u;
    fsi_tx_count = 0u;
    fsi_err_count = 0u;
    uart_ack_count = 0u;
    uart_next_rx_byte = 1u;
    uart_try_read_count = 0u;
    uart_try_write_count = 0u;
    i2c_ack_count = 0u;
    i2c_submit_count = 0u;
    i2c_consume_count = 0u;
    i2c_next_completion_event = SCH_I2C_EVENT_TRANSFER_DONE;
    motor_ack_count = 0u;
    motor_next_speed = 10u;
    motor_read_feedback_count = 0u;
    motor_set_enable_count = 0u;
    motor_set_speed_count = 0u;
    ipc_ack_count = 0u;
    ipc_notify_count = 0u;
    ipc_service_ack_count = 0u;
    ipc_fault_count = 0u;
}

void tearDown(void) {
}

void test_eth_contract_init_validation_and_hint_enqueue_and_bounded_and_empty_and_drop(void) {
    sch_eth_service_t service;
    uint16_t storage[3];
    sch_eth_hal_t hal = {
        .ack_irq = eth_ack_irq,
        .fetch_rx_descriptor = eth_fetch_rx_descriptor,
        .complete_tx_descriptor = eth_complete_tx_descriptor,
        .service_link_state = eth_service_link_state,
        .hal_ctx = NULL,
    };

    TEST_ASSERT_FALSE(sch_eth_service_init(NULL, &hal, storage, 3u, 1u));
    TEST_ASSERT_FALSE(sch_eth_service_init(&service, NULL, storage, 3u, 1u));
    sch_eth_hal_t no_ack = hal;
    no_ack.ack_irq = NULL;
    TEST_ASSERT_FALSE(sch_eth_service_init(&service, &no_ack, storage, 3u, 1u));
    TEST_ASSERT_FALSE(sch_eth_service_init(&service, &hal, storage, 3u, 0u));

    TEST_ASSERT_TRUE(sch_eth_service_init(&service, &hal, storage, 3u, 2u));
    sch_eth_service_run(&service);
    TEST_ASSERT_EQUAL_UINT32(0u, eth_rx_count);
    TEST_ASSERT_EQUAL_UINT32(0u, eth_tx_count);
    TEST_ASSERT_EQUAL_UINT32(0u, eth_link_count);

    sch_eth_isr_rx(&service);
    sch_eth_isr_tx(&service);
    sch_eth_isr_link(&service);
    TEST_ASSERT_TRUE(service.irq_hint);
    TEST_ASSERT_EQUAL_UINT32(3u, (uint32_t)sch_event_queue_size(&service.event_queue));
    TEST_ASSERT_EQUAL_UINT32(3u, eth_ack_count);

    sch_eth_service_run(&service);
    TEST_ASSERT_EQUAL_UINT32(1u, eth_rx_count);
    TEST_ASSERT_EQUAL_UINT32(1u, eth_tx_count);
    TEST_ASSERT_EQUAL_UINT32(0u, eth_link_count);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_event_queue_size(&service.event_queue));

    sch_eth_service_run(&service);
    TEST_ASSERT_EQUAL_UINT32(1u, eth_link_count);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)sch_event_queue_size(&service.event_queue));

    TEST_ASSERT_TRUE(sch_eth_service_init(&service, &hal, storage, 2u, 2u));
    sch_eth_isr_rx(&service);
    sch_eth_isr_tx(&service);
    sch_eth_isr_link(&service);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_event_queue_drop_count(&service.event_queue));
    sch_eth_service_run(&service);
    TEST_ASSERT_EQUAL_UINT32(1u, eth_tx_count);
    TEST_ASSERT_EQUAL_UINT32(2u, eth_link_count);
}

void test_fsi_contract_init_validation_and_hint_enqueue_and_bounded_and_empty_and_drop(void) {
    sch_fsi_service_t service;
    sch_fsi_irq_record_t storage[3];
    sch_fsi_hal_t hal = {
        .ack_irq = fsi_ack_irq,
        .service_rx_frame = fsi_service_rx,
        .service_tx_complete = fsi_service_tx,
        .service_error = fsi_service_error,
        .hal_ctx = NULL,
    };

    TEST_ASSERT_FALSE(sch_fsi_service_init(NULL, &hal, storage, 3u, 1u, SCH_OVERFLOW_DROP_NEWEST));
    TEST_ASSERT_FALSE(sch_fsi_service_init(&service, NULL, storage, 3u, 1u, SCH_OVERFLOW_DROP_NEWEST));
    TEST_ASSERT_FALSE(sch_fsi_service_init(&service, &hal, storage, 3u, 0u, SCH_OVERFLOW_DROP_NEWEST));

    TEST_ASSERT_TRUE(sch_fsi_service_init(&service, &hal, storage, 3u, 2u, SCH_OVERFLOW_DROP_NEWEST));
    sch_fsi_service_run(&service);
    TEST_ASSERT_EQUAL_UINT32(0u, fsi_rx_count);
    TEST_ASSERT_EQUAL_UINT32(0u, fsi_tx_count);
    TEST_ASSERT_EQUAL_UINT32(0u, fsi_err_count);

    sch_fsi_isr_rx(&service, 1u, 2u, 3u);
    sch_fsi_isr_tx(&service, 1u, 2u, 3u);
    sch_fsi_isr_error(&service, 1u, 4u);
    TEST_ASSERT_EQUAL_UINT32(
        SCH_FSI_EVENT_BIT_RX | SCH_FSI_EVENT_BIT_TX | SCH_FSI_EVENT_BIT_ERROR,
        service.event_bits);
    TEST_ASSERT_EQUAL_UINT32(3u, (uint32_t)sch_spsc_ring_size(&service.irq_records));
    TEST_ASSERT_EQUAL_UINT32(3u, fsi_ack_count);

    sch_fsi_service_run(&service);
    TEST_ASSERT_EQUAL_UINT32(1u, fsi_rx_count);
    TEST_ASSERT_EQUAL_UINT32(1u, fsi_tx_count);
    TEST_ASSERT_EQUAL_UINT32(0u, fsi_err_count);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_spsc_ring_size(&service.irq_records));

    sch_fsi_service_run(&service);
    TEST_ASSERT_EQUAL_UINT32(1u, fsi_err_count);

    TEST_ASSERT_TRUE(sch_fsi_service_init(&service, &hal, storage, 2u, 2u, SCH_OVERFLOW_DROP_NEWEST));
    sch_fsi_isr_rx(&service, 1u, 1u, 1u);
    sch_fsi_isr_tx(&service, 1u, 1u, 1u);
    sch_fsi_isr_error(&service, 1u, 1u);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_fsi_service_drop_count(&service));
}

void test_uart_contract_init_validation_and_hint_enqueue_and_bounded_and_empty_and_drop(void) {
    sch_uart_service_t service;
    uint16_t rx_storage[3];
    uint16_t tx_storage[3];
    sch_uart_hal_t hal = {
        .try_read_byte = uart_try_read,
        .try_write_byte = uart_try_write,
        .ack_irq = uart_ack_irq,
        .hal_ctx = NULL,
    };

    TEST_ASSERT_FALSE(sch_uart_service_init(NULL, &hal, rx_storage, 3u, tx_storage, 3u, 1u, 1u));
    TEST_ASSERT_FALSE(sch_uart_service_init(&service, NULL, rx_storage, 3u, tx_storage, 3u, 1u, 1u));
    sch_uart_hal_t missing_read = hal;
    missing_read.try_read_byte = NULL;
    TEST_ASSERT_FALSE(sch_uart_service_init(
        &service, &missing_read, rx_storage, 3u, tx_storage, 3u, 1u, 1u));
    sch_uart_hal_t missing_write = hal;
    missing_write.try_write_byte = NULL;
    TEST_ASSERT_FALSE(sch_uart_service_init(
        &service, &missing_write, rx_storage, 3u, tx_storage, 3u, 1u, 1u));
    TEST_ASSERT_FALSE(sch_uart_service_init(&service, &hal, rx_storage, 3u, tx_storage, 3u, 0u, 1u));
    TEST_ASSERT_FALSE(sch_uart_service_init(&service, &hal, rx_storage, 3u, tx_storage, 3u, 1u, 0u));

    TEST_ASSERT_TRUE(sch_uart_service_init(&service, &hal, rx_storage, 3u, tx_storage, 3u, 1u, 1u));
    sch_uart_service_run(&service);
    TEST_ASSERT_EQUAL_UINT32(0u, uart_try_read_count);
    TEST_ASSERT_EQUAL_UINT32(0u, uart_try_write_count);

    sch_uart_isr_rx(&service);
    sch_uart_isr_tx_ready(&service);
    TEST_ASSERT_TRUE(service.rx_hint);
    TEST_ASSERT_TRUE(service.tx_hint);
    TEST_ASSERT_EQUAL_UINT32(2u, uart_ack_count);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_spsc_ring_size(&service.rx_ring));

    TEST_ASSERT_TRUE(sch_uart_service_queue_tx(&service, 0x11u));
    TEST_ASSERT_TRUE(sch_uart_service_queue_tx(&service, 0x22u));
    sch_uart_isr_rx(&service);
    sch_uart_service_run(&service);
    TEST_ASSERT_EQUAL_UINT32(1u, uart_try_write_count);
    TEST_ASSERT_EQUAL_UINT32(2u, uart_try_read_count);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_spsc_ring_size(&service.tx_ring));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_spsc_ring_size(&service.rx_ring));

    TEST_ASSERT_TRUE(sch_uart_service_init(&service, &hal, rx_storage, 2u, tx_storage, 2u, 1u, 1u));
    TEST_ASSERT_TRUE(sch_uart_service_queue_tx(&service, 1u));
    TEST_ASSERT_TRUE(sch_uart_service_queue_tx(&service, 2u));
    TEST_ASSERT_FALSE(sch_uart_service_queue_tx(&service, 3u));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_spsc_ring_drop_count(&service.tx_ring));

    sch_uart_isr_rx(&service);
    sch_uart_isr_rx(&service);
    sch_uart_isr_rx(&service);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_spsc_ring_drop_count(&service.rx_ring));
}

void test_i2c_contract_init_validation_and_enqueue_hint_bounded_empty_and_drop(void) {
    sch_i2c_eeprom_service_t service;
    sch_eeprom_request_t request_storage[3];
    uint16_t completion_storage[3];
    sch_i2c_eeprom_hal_t hal = {
        .ack_irq = i2c_ack_irq,
        .submit_request = i2c_submit_request,
        .consume_completion = i2c_consume_completion,
        .hal_ctx = NULL,
    };

    TEST_ASSERT_FALSE(sch_i2c_eeprom_service_init(
        NULL,
        &hal,
        request_storage,
        3u,
        completion_storage,
        3u,
        1u,
        1u));
    TEST_ASSERT_FALSE(sch_i2c_eeprom_service_init(
        &service,
        NULL,
        request_storage,
        3u,
        completion_storage,
        3u,
        1u,
        1u));
    sch_i2c_eeprom_hal_t missing_submit = hal;
    missing_submit.submit_request = NULL;
    TEST_ASSERT_FALSE(sch_i2c_eeprom_service_init(
        &service,
        &missing_submit,
        request_storage,
        3u,
        completion_storage,
        3u,
        1u,
        1u));
    TEST_ASSERT_FALSE(sch_i2c_eeprom_service_init(
        &service,
        &hal,
        request_storage,
        3u,
        completion_storage,
        3u,
        0u,
        1u));
    TEST_ASSERT_FALSE(sch_i2c_eeprom_service_init(
        &service,
        &hal,
        request_storage,
        3u,
        completion_storage,
        3u,
        1u,
        0u));

    TEST_ASSERT_TRUE(sch_i2c_eeprom_service_init(
        &service,
        &hal,
        request_storage,
        3u,
        completion_storage,
        3u,
        1u,
        1u));

    sch_i2c_eeprom_service_run(&service);
    TEST_ASSERT_EQUAL_UINT32(0u, i2c_submit_count);

    sch_eeprom_request_t req = {.address = 0x10u, .length = 8u, .write = true};
    TEST_ASSERT_TRUE(sch_i2c_eeprom_enqueue_request_isr(&service, &req));
    sch_i2c_eeprom_isr_complete(&service);
    TEST_ASSERT_TRUE(service.irq_hint);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_spsc_ring_size(&service.request_ring));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_event_queue_size(&service.completion_queue));
    TEST_ASSERT_EQUAL_UINT32(1u, i2c_ack_count);
    TEST_ASSERT_EQUAL_UINT32(1u, i2c_consume_count);

    TEST_ASSERT_TRUE(sch_i2c_eeprom_enqueue_request_isr(&service, &req));
    sch_i2c_eeprom_isr_complete(&service);
    sch_i2c_eeprom_service_run(&service);
    TEST_ASSERT_EQUAL_UINT32(1u, i2c_submit_count);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_spsc_ring_size(&service.request_ring));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_event_queue_size(&service.completion_queue));

    TEST_ASSERT_TRUE(sch_i2c_eeprom_service_init(
        &service,
        &hal,
        request_storage,
        2u,
        completion_storage,
        2u,
        1u,
        1u));
    TEST_ASSERT_TRUE(sch_i2c_eeprom_enqueue_request_isr(&service, &req));
    TEST_ASSERT_TRUE(sch_i2c_eeprom_enqueue_request_isr(&service, &req));
    TEST_ASSERT_FALSE(sch_i2c_eeprom_enqueue_request_isr(&service, &req));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_spsc_ring_drop_count(&service.request_ring));

    sch_i2c_eeprom_isr_complete(&service);
    sch_i2c_eeprom_isr_complete(&service);
    sch_i2c_eeprom_isr_complete(&service);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_event_queue_drop_count(&service.completion_queue));
}

void test_motor_contract_init_validation_and_enqueue_hint_bounded_empty_and_drop(void) {
    sch_motor_supervision_service_t service;
    sch_motor_feedback_t feedback_storage[3];
    uint16_t event_storage[3];
    sch_motor_hal_t hal = {
        .ack_irq = motor_ack_irq,
        .read_feedback_sample = motor_read_feedback_sample,
        .set_enable = motor_set_enable,
        .set_target_speed_rpm = motor_set_target_speed_rpm,
        .hal_ctx = NULL,
    };

    TEST_ASSERT_FALSE(sch_motor_supervision_service_init(
        NULL,
        &hal,
        feedback_storage,
        3u,
        event_storage,
        3u,
        1u,
        1u));
    TEST_ASSERT_FALSE(sch_motor_supervision_service_init(
        &service,
        NULL,
        feedback_storage,
        3u,
        event_storage,
        3u,
        1u,
        1u));
    sch_motor_hal_t missing_enable = hal;
    missing_enable.set_enable = NULL;
    TEST_ASSERT_FALSE(sch_motor_supervision_service_init(
        &service,
        &missing_enable,
        feedback_storage,
        3u,
        event_storage,
        3u,
        1u,
        1u));
    sch_motor_hal_t missing_speed = hal;
    missing_speed.set_target_speed_rpm = NULL;
    TEST_ASSERT_FALSE(sch_motor_supervision_service_init(
        &service,
        &missing_speed,
        feedback_storage,
        3u,
        event_storage,
        3u,
        1u,
        1u));
    TEST_ASSERT_FALSE(sch_motor_supervision_service_init(
        &service,
        &hal,
        feedback_storage,
        3u,
        event_storage,
        3u,
        0u,
        1u));
    TEST_ASSERT_FALSE(sch_motor_supervision_service_init(
        &service,
        &hal,
        feedback_storage,
        3u,
        event_storage,
        3u,
        1u,
        0u));

    TEST_ASSERT_TRUE(sch_motor_supervision_service_init(
        &service,
        &hal,
        feedback_storage,
        3u,
        event_storage,
        3u,
        1u,
        1u));

    sch_motor_supervision_service_run(&service);
    TEST_ASSERT_EQUAL_UINT32(0u, motor_set_enable_count);
    TEST_ASSERT_EQUAL_UINT32(0u, motor_set_speed_count);

    sch_motor_supervision_set_command(&service, true, 1200u);
    sch_motor_isr_feedback(&service);
    sch_motor_isr_feedback(&service);
    sch_motor_isr_fault(&service);
    TEST_ASSERT_TRUE(service.irq_hint);
    TEST_ASSERT_EQUAL_UINT32(3u, motor_ack_count);
    TEST_ASSERT_EQUAL_UINT32(3u, (uint32_t)sch_event_queue_size(&service.event_queue));
    TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)sch_spsc_ring_size(&service.feedback_ring));

    sch_motor_supervision_service_run(&service);
    TEST_ASSERT_EQUAL_UINT32(1u, motor_set_enable_count);
    TEST_ASSERT_EQUAL_UINT32(1u, motor_set_speed_count);
    TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)sch_event_queue_size(&service.event_queue));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_spsc_ring_size(&service.feedback_ring));

    TEST_ASSERT_TRUE(sch_motor_supervision_service_init(
        &service,
        &hal,
        feedback_storage,
        2u,
        event_storage,
        2u,
        1u,
        1u));
    sch_motor_isr_feedback(&service);
    sch_motor_isr_feedback(&service);
    sch_motor_isr_feedback(&service);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_spsc_ring_drop_count(&service.feedback_ring));

    sch_motor_isr_fault(&service);
    sch_motor_isr_fault(&service);
    sch_motor_isr_fault(&service);
    TEST_ASSERT_EQUAL_UINT32(4u, (uint32_t)sch_event_queue_drop_count(&service.event_queue));
}

void test_ipc_contract_init_validation_and_hint_enqueue_and_bounded_and_empty_and_drop(void) {
    sch_ipc_service_t service;
    sch_ipc_irq_record_t storage[3];
    sch_ipc_hal_t hal = {
        .ack_irq = ipc_ack_irq,
        .service_notify = ipc_service_notify,
        .service_ack = ipc_service_ack,
        .service_fault = ipc_service_fault,
        .hal_ctx = NULL,
    };

    TEST_ASSERT_FALSE(sch_ipc_service_init(NULL, &hal, storage, 3u, 1u, SCH_OVERFLOW_DROP_NEWEST));
    TEST_ASSERT_FALSE(sch_ipc_service_init(&service, NULL, storage, 3u, 1u, SCH_OVERFLOW_DROP_NEWEST));
    TEST_ASSERT_FALSE(sch_ipc_service_init(&service, &hal, storage, 3u, 0u, SCH_OVERFLOW_DROP_NEWEST));

    TEST_ASSERT_TRUE(sch_ipc_service_init(&service, &hal, storage, 3u, 2u, SCH_OVERFLOW_DROP_NEWEST));
    sch_ipc_service_run(&service);
    TEST_ASSERT_EQUAL_UINT32(0u, ipc_notify_count);
    TEST_ASSERT_EQUAL_UINT32(0u, ipc_service_ack_count);
    TEST_ASSERT_EQUAL_UINT32(0u, ipc_fault_count);

    sch_ipc_isr_notify(&service, 1u, 1u, 1u);
    sch_ipc_isr_ack(&service, 1u, 1u, 2u);
    sch_ipc_isr_fault(&service, 1u, 3u, 2u);
    TEST_ASSERT_EQUAL_UINT32(
        SCH_IPC_EVENT_BIT_NOTIFY | SCH_IPC_EVENT_BIT_ACK | SCH_IPC_EVENT_BIT_FAULT,
        service.event_bits);
    TEST_ASSERT_EQUAL_UINT32(3u, (uint32_t)sch_spsc_ring_size(&service.irq_records));
    TEST_ASSERT_EQUAL_UINT32(3u, ipc_ack_count);

    sch_ipc_service_run(&service);
    TEST_ASSERT_EQUAL_UINT32(1u, ipc_notify_count);
    TEST_ASSERT_EQUAL_UINT32(1u, ipc_service_ack_count);
    TEST_ASSERT_EQUAL_UINT32(0u, ipc_fault_count);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_spsc_ring_size(&service.irq_records));

    sch_ipc_service_run(&service);
    TEST_ASSERT_EQUAL_UINT32(1u, ipc_fault_count);

    TEST_ASSERT_TRUE(sch_ipc_service_init(&service, &hal, storage, 2u, 2u, SCH_OVERFLOW_DROP_NEWEST));
    sch_ipc_isr_notify(&service, 1u, 1u, 1u);
    sch_ipc_isr_ack(&service, 1u, 1u, 1u);
    sch_ipc_isr_fault(&service, 1u, 1u, 1u);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sch_ipc_service_drop_count(&service));
}
