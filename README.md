# Cooperative bare-metal scheduler library (C11)

A small, portable **cooperative scheduler** for embedded systems.

This library lets you register periodic and background tasks, then run them in a deterministic run-to-completion loop. It is designed for bare-metal projects that do not need preemption or a full RTOS.

## Why this library

- Deterministic cooperative scheduling (no preemption).
- Static task model with no dynamic allocation required by the scheduler.
- Low integration overhead for superloop firmware.
- Portable architecture via a compact porting interface.

## Quick start (5 minutes)

### 1) Add source + headers

At minimum include:

- `src/scheduler.c`
- `include/` in your include paths

### 2) Implement port hooks (`scheduler_port.h`)

You must provide:

- `uint32_t sch_port_now_ticks(void)`
- `uint32_t sch_port_enter_critical(void)`
- `void sch_port_exit_critical(uint32_t state)`
- `void sch_port_idle(void)`

Typical mapping:

- `sch_port_now_ticks`: monotonic hardware timer/counter or ISR-maintained tick.
- `sch_port_enter_critical` / `sch_port_exit_critical`: disable/restore interrupts (or equivalent).
- `sch_port_idle`: low-power wait (`WFI`) or no-op.

> **Important:** `sch_port_now_ticks()` should be based on a real time source in production firmware.

### 3) Register tasks and run scheduler

```c
#include "scheduler/scheduler.h"

static void control_task(void *ctx) {
    (void)ctx;
    /* control work */
}

static void diag_task(void *ctx) {
    (void)ctx;
    /* diagnostics work */
}

int main(void) {
    sch_t scheduler;

    sch_init(&scheduler);

    (void)sch_add_task(&scheduler, control_task, NULL, 1000u, 0u, 0u);
    (void)sch_add_task(&scheduler, diag_task, NULL, 5000u, 0u, 10u);

    for (;;) {
        sch_run(&scheduler);
    }
}
```

## Core concepts

- **Periodic task:** `period_ticks > 0`.
- **Background task:** `period_ticks == 0` (runs when no periodic task is ready).
- **Priority:** used when multiple tasks are ready at once.
- **Run-to-completion:** callbacks should be bounded and non-blocking.

## Configuration reference

| Macro | Default | Meaning |
|---|---:|---|
| `SCH_MAX_TASKS` | project-defined | Max statically allocated scheduler task slots. |
| `SCH_ENABLE_STATS` | `0` | Enables per-task execution counters/timestamps. |
| `SCH_ENABLE_TRACE` | `0` | Enables trace callbacks for task/scheduler events. |

With stats/trace disabled, instrumentation code paths are compiled out.

## Service skeleton modules (`include/scheduler/services/`)

All service modules in this folder use the same core contract:

- ISR path is minimal and bounded (ack + capture + queue + hint).
- Service path drains bounded work per scheduler run.
- Queue/ring carries multiplicity/payload while event bits are advisory wakeups.

Available skeletons:

- `uart_service`: byte-oriented RX/TX handoff using SPSC rings.
- `ethernet_service`: RX/TX/link events via event queue descriptors.
- `i2c_eeprom_service`: deferred request submission + completion event handling.
- `motor_supervision_service`: feedback sampling + fault supervision.
- `fsi_service`: FSI RX/TX/error interrupt handoff with compact ISR records.
- `ipc_service`: IPC notify/ack/fault interrupt handoff with compact ISR records.

## Build (CMake)

```bash
cmake -S . -B build
cmake --build build
./build/main_example
./build/main_events_example
```

`main_events_example` is a finite runtime demo (about 120 ms) that shows:

- 1 ms control task,
- periodic polling-first service tasks,
- simulated ISR burst production into ring/event queues,
- bounded service budgets per run,
- background diagnostics with counters (processed, empty polls, drops).

## Test (Ceedling)

```bash
ceedling test:all
```

## CMake vs Ceedling

- **Keep building with CMake** for normal project builds.
- **Ceedling is a separate unit-test build system** that compiles test runners and selected production sources from `project.yml`.
- `ceedling test:all` compiles and runs tests independently of your CMake build.

## Troubleshooting

- **Timing looks unstable:** verify `sch_port_now_ticks()` is monotonic.
- **Tasks miss expected cadence:** reduce callback runtime and review priorities.
- **System appears busy/spins:** check background task behavior and `sch_port_idle()` mapping.
- **ISR-to-service lag:** verify queue budgets and drain rate in service tasks.

## Project structure

- `include/scheduler/scheduler.h`: public scheduler API and task model.
- `include/scheduler/isr_buffer.h`: ISR-producer/task-consumer ring and event queue utilities.
- `include/scheduler/services/*.h`: service skeleton module APIs.
- `include/scheduler/port/scheduler_port.h`: platform/porting hooks.
- `src/scheduler.c`: scheduler implementation.
- `src/isr_buffer.c`: buffering utility implementation.
- `src/services/*.c`: service skeleton implementations.
- `examples/main_example.c`: minimal integration example.
- `tests/unit/test_scheduler.c`: unit tests (Unity/Ceedling).
