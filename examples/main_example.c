/**
 * @file main_example.c
 * @brief Minimal integration example for the cooperative scheduler.
 */

#include "scheduler/scheduler.h"
#include "scheduler/port/scheduler_port.h"

#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

/** @brief Simulated monotonic tick source in microseconds. */
static volatile uint32_t g_time_us = 0u;
/** @brief Reference wall-clock timestamp used to derive relative scheduler ticks. */
static struct timespec g_start_time = {0};

/**
 * @brief Read current wall-clock time in microseconds.
 *
 * @return Current wall-clock timestamp converted to microseconds.
 */
static uint64_t get_now_us(void) {
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        return 0u;
    }

    return ((uint64_t)tv.tv_sec * 1000000ull) + (uint64_t)tv.tv_usec;
}

/**
 * @brief Example high-priority control task.
 *
 * @param ctx Unused callback context.
 */
static void control_task(void *ctx) {
    (void)ctx;
    (void)printf("I am in control task\n\r");
}

/**
 * @brief Example lower-priority diagnostics task.
 *
 * @param ctx Unused callback context.
 */
static void diagnostics_task(void *ctx) {
    (void)ctx;
    (void)printf("I am in diagnostics task\n\r");
}

/**
 * @brief Example entry point.
 *
 * @return Always returns 0.
 */
int main(void) {
    sch_t scheduler;
    const uint64_t start_time_us = get_now_us();

    g_start_time.tv_sec = (time_t)(start_time_us / 1000000ull);
    g_start_time.tv_nsec = (long)((start_time_us % 1000000ull) * 1000ull);

    sch_init(&scheduler);

    (void)sch_add_task(&scheduler, control_task, NULL, 1000000u, 1000u, 0u);
    (void)sch_add_task(&scheduler, diagnostics_task, NULL, 2000000u, 0u, 20u);

    for (;;) {
        sch_run(&scheduler);
    }

    return 0;
}

/** @brief Read simulated scheduler tick count. */
uint32_t sch_port_now_ticks(void) {
    const uint64_t now_us = get_now_us();
    const uint64_t start_us = ((uint64_t)g_start_time.tv_sec * 1000000ull) +
                              ((uint64_t)g_start_time.tv_nsec / 1000ull);

    if (now_us < start_us) {
        g_time_us = 0u;
    } else {
        g_time_us = (uint32_t)(now_us - start_us);
    }

    return g_time_us;
}

/** @brief Enter simulated critical section. */
uint32_t sch_port_enter_critical(void) {
    return 0u;
}

/** @brief Exit simulated critical section. */
void sch_port_exit_critical(uint32_t state) {
    (void)state;
}

/** @brief Execute simulated idle behavior. */
void sch_port_idle(void) {
}
