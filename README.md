# Cooperative bare-metal scheduler library (C99)

A small, portable **cooperative scheduler** for embedded systems.

This library lets you register periodic and background tasks, then run them in a deterministic run-to-completion loop. It is designed for bare-metal projects that do not need preemption or a full RTOS.

## Documentation authority and planning artifacts

- This README is the **authoritative source** for the library's currently supported behavior and current skeleton-module limitations.
- Files under `docs/codex_instructions/` are **design/backlog artifacts** used to guide implementation planning and AI tasking. They may include historical TODO items that are already complete or items intentionally not implemented in the current architecture.
- Release readiness checklist: see `docs/release_readiness.md` before claiming production readiness.

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


### Porting

See [`docs/porting_contract.md`](docs/porting_contract.md) for the full contract.

Bring-up checklist (C28x / R5F / A53-style targets):

- Provide a monotonic `sch_port_now_ticks()` source (wrap-safe `uint32_t`).
- Implement `sch_port_enter_critical` / `sch_port_exit_critical` as a paired,
  nested-safe state restore (never unconditional `enable_irq`).
- Ensure ISR-safe critical calls for ISR producer APIs and block all IRQ
  priorities that can touch scheduler-shared state.
- Add required ordering/barriers so ISR/task handoff is visible on weaker
  memory-order systems.
- Keep `sch_port_idle()` wake-safe (`WFI`/wait path must still wake on expected
  interrupts).

Board validation quick checks:

- Tick monotonicity under load.
- Critical nesting behavior (enter twice, exit twice, interrupts only restore on
  outermost exit).
- Idle safety and expected wakeups.

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

## API behavior policy: no runtime disable support

The `sch_enable_task()` API is preserved as a **compatibility shim** only.

- Calling `sch_enable_task(..., true)` does not change scheduler behavior.
- Calling `sch_enable_task(..., false)` also does not disable a task.
- This is intentional: **disable is intentionally unsupported by design**.

### Rationale

- Keeps scheduler behavior deterministic and simple for safety-oriented superloop use.
- Avoids runtime mode changes that can hide periodic/background work unexpectedly.
- Preserves source compatibility for projects that already call a legacy enable/disable API.

If you need conditional execution, keep tasks registered and gate work inside the task callback using application-owned state.

## Configuration reference

| Macro | Default | Meaning |
|---|---:|---|
| `SCH_MAX_TASKS` | `64` (fixed) | Max statically allocated scheduler task slots. |
| `SCH_ENABLE_STATS` | `0` | Enables per-task execution counters/timestamps. |
| `SCH_ENABLE_TRACE` | `0` | Enables trace callbacks for task/scheduler events. |

`SCH_MAX_TASKS` is intentionally fixed to a deterministic static size to preserve embedded-time predictability (bounded memory and iteration costs).

With stats/trace disabled, instrumentation code paths are compiled out.

### Statistics and trace capabilities

The scheduler library includes two optional instrumentation features:

- **Statistics (`SCH_ENABLE_STATS`)**
  - Per-task counters/timestamps for execution behavior.
  - Useful for measuring task activity, runtime cadence, and troubleshooting timing behavior during development.
- **Trace hooks (`SCH_ENABLE_TRACE`)**
  - Callback hooks for key scheduler/task lifecycle events.
  - Useful for event-level observability (for example, feeding debug logs, timeline traces, or external diagnostics).

Because both features are compile-time gated, they are disabled by default and add no instrumentation overhead unless explicitly enabled.

#### How to enable with CMake

Enable either or both options at configure time:

```bash
cmake -S . -B build -DSCH_ENABLE_STATS=ON -DSCH_ENABLE_TRACE=ON
cmake --build build
```

You can also enable only one feature:

```bash
cmake -S . -B build -DSCH_ENABLE_STATS=ON
cmake -S . -B build -DSCH_ENABLE_TRACE=ON
```

If `build/` already exists, rerun the `cmake -S . -B build -D...` configure command to update cached options, then rebuild.

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
