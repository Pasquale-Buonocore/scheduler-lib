/**
 * @file scheduler_port.h
 * @brief Platform abstraction hooks required by the scheduler.
 *
 * Port implementations must satisfy the semantics defined in
 * `docs/porting_contract.md`.
 */

#ifndef SCHEDULER_PORT_SCHEDULER_PORT_H_
#define SCHEDULER_PORT_SCHEDULER_PORT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read current platform tick count.
 *
 * Must return a monotonic `uint32_t` tick source suitable for wrap-safe
 * scheduler comparisons.
 *
 * @return Current scheduler tick count.
 */
uint32_t sch_port_now_ticks(void);

/**
 * @brief Enter a critical section.
 *
 * Must mask/prevent concurrent ISR updates to scheduler-shared state,
 * support nesting, and return a token that captures the pre-entry interrupt
 * state for exact restoration by @ref sch_port_exit_critical.
 *
 * @return Platform-specific state used to restore critical state.
 */
uint32_t sch_port_enter_critical(void);

/**
 * @brief Exit a critical section.
 *
 * Must restore the exact state token returned by
 * @ref sch_port_enter_critical, preserve nesting correctness, and ensure
 * critical-section writes are visible before interrupts resume.
 *
 * @param state State token returned by @ref sch_port_enter_critical.
 */
void sch_port_exit_critical(uint32_t state);

/**
 * @brief Execute idle behavior when no task is ready.
 *
 * Must be safe for scheduler bring-up (no deadlock/lost wakeup).
 */
void sch_port_idle(void);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_PORT_SCHEDULER_PORT_H_ */
