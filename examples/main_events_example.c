/**
 * @file main_events_example.c
 * @brief Polling-first hybrid example with control, service, and background tasks.
 */

#include "scheduler/isr_buffer.h"
#include "scheduler/port/scheduler_port.h"
#include "scheduler/scheduler.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#define DEMO_RUNTIME_MS (120u)
#define UART_RING_CAPACITY (8u)
#define ETH_EVENT_CAPACITY (6u)

typedef enum {
    DEMO_ETH_RX_READY = 1,
    DEMO_ETH_TX_DONE = 2,
    DEMO_ETH_LINK_CHANGE = 3
} demo_eth_event_t;

typedef struct {
    sch_spsc_ring_t uart_rx_ring;
    uint16_t uart_rx_storage[UART_RING_CAPACITY];

    sch_event_queue_t eth_event_queue;
    uint16_t eth_event_storage[ETH_EVENT_CAPACITY];

    volatile bool uart_irq_hint;
    volatile bool eth_irq_hint;

    uint32_t control_runs;
    uint32_t uart_empty_polls;
    uint32_t uart_processed;
    uint32_t eth_empty_polls;
    uint32_t eth_processed;
    uint32_t background_runs;

    uint32_t isr_uart_generated;
    uint32_t isr_eth_generated;
    uint16_t next_uart_byte;
    uint16_t next_eth_event;

    uint32_t last_report_tick;
} demo_app_t;

static struct timeval g_start_time = {0};

static uint64_t sch_mono_now_us(void) {
    struct timeval now = {0};
    (void)gettimeofday(&now, NULL);
    return ((uint64_t)now.tv_sec * 1000000ull) + (uint64_t)now.tv_usec;
}

static void control_task(void *ctx) {
    demo_app_t *app = (demo_app_t *)ctx;
    app->control_runs++;
}

static void simulated_isr_push_uart(demo_app_t *app) {
    uint16_t byte = app->next_uart_byte++;
    (void)sch_spsc_ring_push_isr(&app->uart_rx_ring, &byte);
    app->uart_irq_hint = true;
    app->isr_uart_generated++;
}

static void simulated_isr_push_eth_event(demo_app_t *app) {
    uint16_t event = (uint16_t)app->next_eth_event;
    app->next_eth_event++;
    if (app->next_eth_event > DEMO_ETH_LINK_CHANGE) {
        app->next_eth_event = DEMO_ETH_RX_READY;
    }

    (void)sch_event_queue_push_isr(&app->eth_event_queue, event);
    app->eth_irq_hint = true;
    app->isr_eth_generated++;
}

static void simulated_isr_task(void *ctx) {
    demo_app_t *app = (demo_app_t *)ctx;

    /* Burst arrivals intentionally exceed service budgets to demonstrate bounded work and drops. */
    for (uint16_t i = 0u; i < 3u; ++i) {
        simulated_isr_push_uart(app);
    }

    for (uint16_t i = 0u; i < 2u; ++i) {
        simulated_isr_push_eth_event(app);
    }
}

static void uart_service_task(void *ctx) {
    demo_app_t *app = (demo_app_t *)ctx;
    const size_t max_items_per_run = 2u;

    if (!app->uart_irq_hint && sch_spsc_ring_is_empty(&app->uart_rx_ring)) {
        app->uart_empty_polls++;
        return;
    }

    app->uart_irq_hint = false;

    uint16_t byte = 0u;
    for (size_t i = 0u; i < max_items_per_run; ++i) {
        if (!sch_spsc_ring_pop_task(&app->uart_rx_ring, &byte)) {
            break;
        }

        (void)byte;
        app->uart_processed++;
    }
}

static void eth_service_task(void *ctx) {
    demo_app_t *app = (demo_app_t *)ctx;
    const size_t max_events_per_run = 1u;

    if (!app->eth_irq_hint && (sch_event_queue_size(&app->eth_event_queue) == 0u)) {
        app->eth_empty_polls++;
        return;
    }

    app->eth_irq_hint = false;

    uint16_t event = 0u;
    for (size_t i = 0u; i < max_events_per_run; ++i) {
        if (!sch_event_queue_pop_task(&app->eth_event_queue, &event)) {
            break;
        }

        (void)event;
        app->eth_processed++;
    }
}

static void background_task(void *ctx) {
    demo_app_t *app = (demo_app_t *)ctx;
    const uint32_t now = sch_port_now_ticks();

    app->background_runs++;

    if ((now - app->last_report_tick) < 20u * 1000u) {
        return;
    }

    app->last_report_tick = now;

    (void)printf(
        "t=%6u us | control=%3u | uart(proc=%3u empty=%3u q=%zu drop=%zu) | "
        "eth(proc=%3u empty=%3u q=%zu drop=%zu)\n",
        now,
        app->control_runs,
        app->uart_processed,
        app->uart_empty_polls,
        sch_spsc_ring_size(&app->uart_rx_ring),
        sch_spsc_ring_drop_count(&app->uart_rx_ring),
        app->eth_processed,
        app->eth_empty_polls,
        sch_event_queue_size(&app->eth_event_queue),
        sch_event_queue_drop_count(&app->eth_event_queue));
}

uint32_t sch_port_now_ticks(void) {
    const uint64_t now_us = sch_mono_now_us();
    const uint64_t start_us = ((uint64_t)g_start_time.tv_sec * 1000000ull) +
                              (uint64_t)g_start_time.tv_usec;

    if (now_us < start_us) {
        return 0u;
    }

    return (uint32_t)(now_us - start_us);
}

uint32_t sch_port_enter_critical(void) {
    return 0u;
}

void sch_port_exit_critical(uint32_t state) {
    (void)state;
}

void sch_port_idle(void) {
}

int main(void) {
    sch_t scheduler;
    demo_app_t app = {0};
    const uint64_t start_us = sch_mono_now_us();

    g_start_time.tv_sec = (time_t)(start_us / 1000000ull);
    g_start_time.tv_usec = (long)(start_us % 1000000ull);

    (void)sch_spsc_ring_init(
        &app.uart_rx_ring,
        app.uart_rx_storage,
        UART_RING_CAPACITY,
        sizeof(app.uart_rx_storage[0]),
        SCH_OVERFLOW_DROP_OLDEST);
    (void)sch_event_queue_init(
        &app.eth_event_queue, app.eth_event_storage, ETH_EVENT_CAPACITY, SCH_OVERFLOW_DROP_OLDEST);
    app.next_uart_byte = 1u;
    app.next_eth_event = DEMO_ETH_RX_READY;

    sch_init(&scheduler);

    (void)sch_add_task(&scheduler, control_task, &app, 1000u, 0u, 0u);
    (void)sch_add_task(&scheduler, simulated_isr_task, &app, 3000u, 500u, 1u);
    (void)sch_add_task(&scheduler, uart_service_task, &app, 2000u, 0u, 5u);
    (void)sch_add_task(&scheduler, eth_service_task, &app, 5000u, 0u, 8u);
    (void)sch_add_task(&scheduler, background_task, &app, 0u, 0u, 50u);

    while (sch_port_now_ticks() < (DEMO_RUNTIME_MS * 1000u)) {
        sch_run(&scheduler);
    }

    (void)printf("\n--- final summary ---\n");
    (void)printf("control runs          : %u\n", app.control_runs);
    (void)printf("uart generated        : %u\n", app.isr_uart_generated);
    (void)printf("uart processed        : %u\n", app.uart_processed);
    (void)printf("uart empty polls      : %u\n", app.uart_empty_polls);
    (void)printf("uart ring drops       : %zu\n", sch_spsc_ring_drop_count(&app.uart_rx_ring));
    (void)printf("eth generated         : %u\n", app.isr_eth_generated);
    (void)printf("eth processed         : %u\n", app.eth_processed);
    (void)printf("eth empty polls       : %u\n", app.eth_empty_polls);
    (void)printf("eth queue drops       : %zu\n", sch_event_queue_drop_count(&app.eth_event_queue));
    (void)printf("background iterations : %u\n", app.background_runs);

    return 0;
}
