/**
 * @file test_service_shared_state.c
 * @brief Unit tests for ISR/task shared wake hints and advisory event bits.
 */

#include "unity.h"

#include "scheduler/services/ethernet_service.h"
#include "scheduler/services/fsi_service.h"
#include "scheduler/services/uart_service.h"

/** @brief Port stub: fake critical section nesting counter for coverage. */
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

static uint32_t eth_ack_count;
static uint32_t eth_rx_service_count;

static void eth_ack_irq(void *ctx) {
    (void)ctx;
    eth_ack_count++;
}

static bool eth_fetch_rx_descriptor(void *ctx, uint16_t *out_desc) {
    (void)ctx;
    if (out_desc != NULL) {
        *out_desc = (uint16_t)eth_rx_service_count;
    }
    eth_rx_service_count++;
    return true;
}

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

static uint32_t uart_ack_count;
static uint32_t uart_rx_next;
static uint32_t uart_tx_count;

static void uart_ack_irq(void *ctx) {
    (void)ctx;
    uart_ack_count++;
}

static bool uart_try_read(void *ctx, uint16_t *out_byte) {
    (void)ctx;
    if (out_byte != NULL) {
        *out_byte = (uint16_t)uart_rx_next;
    }
    uart_rx_next++;
    return true;
}

static bool uart_try_write(void *ctx, uint16_t byte) {
    (void)ctx;
    (void)byte;
    uart_tx_count++;
    return true;
}

void setUp(void) {
    critical_depth = 0u;
    eth_ack_count = 0u;
    eth_rx_service_count = 0u;
    fsi_ack_count = 0u;
    fsi_rx_count = 0u;
    fsi_tx_count = 0u;
    fsi_err_count = 0u;
    uart_ack_count = 0u;
    uart_rx_next = 0u;
    uart_tx_count = 0u;
}

void tearDown(void) {
}

void test_ethernet_irq_hint_should_not_lose_events_across_burst_and_alternating_runs(void) {
    sch_eth_service_t service;
    uint16_t event_storage[64];
    sch_eth_hal_t hal = {
        .ack_irq = eth_ack_irq,
        .fetch_rx_descriptor = eth_fetch_rx_descriptor,
        .complete_tx_descriptor = NULL,
        .service_link_state = NULL,
        .hal_ctx = NULL,
    };

    TEST_ASSERT_TRUE(sch_eth_service_init(&service, &hal, event_storage, 64u, 1u));

    for (uint32_t i = 0u; i < 32u; ++i) {
        sch_eth_service_run(&service);
        sch_eth_isr_rx(&service);
        sch_eth_service_run(&service);
    }

    for (uint32_t i = 0u; i < 16u; ++i) {
        sch_eth_isr_rx(&service);
        sch_eth_isr_rx(&service);
        sch_eth_isr_rx(&service);
        sch_eth_service_run(&service);
        sch_eth_service_run(&service);
        sch_eth_service_run(&service);
    }

    TEST_ASSERT_EQUAL_UINT32(80u, eth_ack_count);
    TEST_ASSERT_EQUAL_UINT32(80u, eth_rx_service_count);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)sch_event_queue_size(&service.event_queue));
}

void test_fsi_event_bits_should_not_drop_records_during_repeated_isr_task_alternation(void) {
    sch_fsi_service_t service;
    sch_fsi_irq_record_t record_storage[128];
    sch_fsi_hal_t hal = {
        .ack_irq = fsi_ack_irq,
        .service_rx_frame = fsi_service_rx,
        .service_tx_complete = fsi_service_tx,
        .service_error = fsi_service_error,
        .hal_ctx = NULL,
    };

    TEST_ASSERT_TRUE(sch_fsi_service_init(
        &service,
        &hal,
        record_storage,
        128u,
        2u,
        SCH_OVERFLOW_DROP_NEWEST));

    for (uint32_t i = 0u; i < 40u; ++i) {
        sch_fsi_service_run(&service);
        sch_fsi_isr_rx(&service, (uint16_t)i, (uint16_t)(i + 1u), 0x10u);
        sch_fsi_isr_tx(&service, (uint16_t)i, (uint16_t)(i + 2u), 0x20u);
        sch_fsi_isr_error(&service, (uint16_t)i, 0x40u);
        sch_fsi_service_run(&service);
        sch_fsi_service_run(&service);
    }

    for (uint32_t i = 0u; i < 20u; ++i) {
        sch_fsi_service_run(&service);
    }

    TEST_ASSERT_EQUAL_UINT32(120u, fsi_ack_count);
    TEST_ASSERT_EQUAL_UINT32(40u, fsi_rx_count);
    TEST_ASSERT_EQUAL_UINT32(40u, fsi_tx_count);
    TEST_ASSERT_EQUAL_UINT32(40u, fsi_err_count);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)sch_spsc_ring_size(&service.irq_records));
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)sch_fsi_service_drop_count(&service));
}

void test_uart_rx_tx_hints_should_preserve_wakeups_under_repeated_isr_task_alternation(void) {
    sch_uart_service_t service;
    uint16_t rx_storage[64];
    uint16_t tx_storage[64];
    sch_uart_hal_t hal = {
        .try_read_byte = uart_try_read,
        .try_write_byte = uart_try_write,
        .ack_irq = uart_ack_irq,
        .hal_ctx = NULL,
    };

    TEST_ASSERT_TRUE(sch_uart_service_init(
        &service,
        &hal,
        rx_storage,
        64u,
        tx_storage,
        64u,
        4u,
        4u));

    for (uint16_t i = 0u; i < 96u; ++i) {
        TEST_ASSERT_TRUE(sch_uart_service_queue_tx(&service, i));
        sch_uart_isr_tx_ready(&service);
        sch_uart_isr_rx(&service);
        sch_uart_service_run(&service);
    }

    TEST_ASSERT_EQUAL_UINT32(192u, uart_ack_count);
    TEST_ASSERT_EQUAL_UINT32(96u, uart_tx_count);
    TEST_ASSERT_TRUE(sch_spsc_ring_is_empty(&service.tx_ring));
    TEST_ASSERT_TRUE(sch_spsc_ring_is_empty(&service.rx_ring));
}
