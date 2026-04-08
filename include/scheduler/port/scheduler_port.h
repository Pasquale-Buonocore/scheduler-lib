/**
 * @file scheduler_port.h
 * @brief Platform abstraction hooks required by the scheduler.
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
 * @return Current scheduler tick count.
 */
uint32_t sch_port_now_ticks(void);

/**
 * @brief Enter a critical section.
 *
 * @return Platform-specific state used to restore critical state.
 */
uint32_t sch_port_enter_critical(void);

/**
 * @brief Exit a critical section.
 *
 * @param state State token returned by @ref sch_port_enter_critical.
 */
void sch_port_exit_critical(uint32_t state);

/**
 * @brief Execute idle behavior when no task is ready.
 */
void sch_port_idle(void);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_PORT_SCHEDULER_PORT_H_ */
