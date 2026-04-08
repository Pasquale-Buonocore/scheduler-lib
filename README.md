# Cooperative bare-metal scheduler library (C11)

A small, portable **cooperative scheduler** for embedded systems.

This library lets you register periodic and background tasks, then run them in a deterministic run-to-completion loop. It is designed for bare-metal projects that do not need preemption or a full RTOS.

## What the library does

- Runs tasks cooperatively (no preemption).
- Supports up to `SCH_MAX_TASKS` statically allocated tasks.
- Schedules periodic tasks using an overflow-safe 32-bit tick timeline.
- Supports background tasks (`period_ticks == 0`) that run when no periodic task is ready.
- Delegates hardware/platform specifics (time source, critical sections, idle behavior) through a porting layer.

## When to use it

Use this scheduler when you want:

- predictable task execution order,
- no dynamic memory,
- simple integration in a bare-metal main loop,
- and low overhead compared to a full RTOS.

## Project structure

- `include/scheduler/scheduler.h`: public scheduler API and task model.
- `include/scheduler/isr_buffer.h`: ISR-producer/task-consumer ring and event queue utilities.
- `include/scheduler/services/uart_service.h`: UART ISR-to-service skeleton.
- `include/scheduler/services/ethernet_service.h`: Ethernet ISR-to-service skeleton.
- `include/scheduler/services/i2c_eeprom_service.h`: I2C/EEPROM request/completion skeleton.
- `include/scheduler/services/motor_supervision_service.h`: motor feedback/fault supervision skeleton.
- `include/scheduler/services/fsi_service.h`: FSI ISR-to-service skeleton.
- `include/scheduler/services/ipc_service.h`: IPC ISR-to-service skeleton.
- `include/scheduler/port/scheduler_port.h`: platform/porting hooks.
- `src/scheduler.c`: scheduler implementation.
- `src/isr_buffer.c`: buffering utility implementation.
- `src/services/*.c`: service skeleton implementations for UART/Ethernet/I2C/Motor/FSI/IPC.
- `examples/main_example.c`: minimal integration example.
- `tests/unit/test_scheduler.c`: unit tests (Unity/Ceedling).

## Service skeleton modules (`include/scheduler/services/`)

All service modules in this folder use the same core contract:
- ISR path is bounded and minimal (ack + capture + queue + hint).
- Service path performs bounded draining/processing per scheduler run.
- Queue/ring carries multiplicity/payload while hint/event bits are advisory wakeups.

Available skeletons:
- `uart_service`: byte-oriented RX/TX handoff using SPSC rings.
- `ethernet_service`: RX/TX/link events via event queue descriptors.
- `i2c_eeprom_service`: deferred request submission + completion event handling.
- `motor_supervision_service`: feedback sampling + fault event supervision.
- `fsi_service`: FSI RX/TX/error interrupt handoff with compact ISR records.
- `ipc_service`: IPC notify/ack/fault interrupt handoff with compact ISR records.

## FSI/IPC interrupt service skeleton wiring

Both FSI and IPC skeletons follow the same ISR-to-task handoff model:
- ISR acknowledges source, records minimal metadata, sets advisory event bits.
- Service task drains a bounded number of queued records per scheduler run.

```c
#include "scheduler/services/fsi_service.h"
#include "scheduler/services/ipc_service.h"

static sch_fsi_service_t g_fsi_service;
static sch_ipc_service_t g_ipc_service;

/* Register as background/event service tasks. */
(void)sch_add_task(&scheduler, sch_fsi_service_run, &g_fsi_service, 0u, 0u, 6u);
(void)sch_add_task(&scheduler, sch_ipc_service_run, &g_ipc_service, 0u, 0u, 7u);

/* Platform IRQ stubs route to minimal handoff ISRs. */
void FSI_RX_IRQHandler(void) {
    sch_fsi_isr_rx(&g_fsi_service, 0u, 0u, 0u);
}

void IPC_NOTIFY_IRQHandler(void) {
    sch_ipc_isr_notify(&g_ipc_service, 0u, 0u, 0u);
}
```

## Core integration steps for an embedded project

### 1) Add the library sources and headers to your build

At minimum include:

- `src/scheduler.c`
- `include/` in your include paths

### 2) Implement the platform port functions

You must provide these functions declared in `scheduler_port.h`:

- `uint32_t sch_port_now_ticks(void)`
- `uint32_t sch_port_enter_critical(void)`
- `void sch_port_exit_critical(uint32_t state)`
- `void sch_port_idle(void)`

Typical embedded mapping:

- `sch_port_now_ticks`: read a monotonic hardware timer/counter (or ISR-maintained tick counter).
- `sch_port_enter_critical` / `sch_port_exit_critical`: disable/restore interrupts or equivalent protection.
- `sch_port_idle`: execute low-power wait (`WFI`) or a no-op.

> **Important:** `sch_port_now_ticks()` should be based on a real time source in production firmware, not on loop-iteration counting.

### 3) Initialize and register tasks

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

    /* period_ticks is in your port tick unit */
    (void)sch_add_task(&scheduler, control_task, NULL, 1000u, 0u, 0u);
    (void)sch_add_task(&scheduler, diag_task, NULL, 5000u, 0u, 10u);

    for (;;) {
        sch_run(&scheduler);
    }
}
```

### 4) Run scheduler continuously in main loop

Call `sch_run()` repeatedly from the superloop. The scheduler will execute whichever task is ready according to:

- next-release time,
- priority,
- and task configuration.

## Scheduling notes

- Task callbacks run to completion and must not block for long durations.
- Keep callback execution bounded to preserve timing for other tasks.
- If several tasks are ready at once, priority affects selection.
- Use a stable, monotonic tick source for predictable behavior.

## Optional instrumentation and trace hooks

The scheduler supports compile-time-gated observability features:

- `SCH_ENABLE_STATS=1`: enables per-task counters/timestamps.
- `SCH_ENABLE_TRACE=1`: enables trace callbacks for task start/end, miss,
  overrun, and idle events.

Both gates default to `0` and are fully compiled out when disabled.

### CMake feature toggles

You can enable these options from CMake:

```bash
cmake -S . -B build-instrumented -DSCH_ENABLE_STATS=ON -DSCH_ENABLE_TRACE=ON
cmake --build build-instrumented
```

### Direct compiler defines (non-CMake integration)

If you integrate sources into another embedded build system, define:

```c
-DSCH_ENABLE_STATS=1
-DSCH_ENABLE_TRACE=1
```

### Stats API (enabled only with `SCH_ENABLE_STATS=1`)

- `sch_get_task_stats(...)` returns a copy of per-task stats.
- `sch_reset_stats(...)` clears all task stats in a scheduler instance.

Per-task fields:

- `run_count`, `miss_count`, `overrun_count`
- `last_start_tick`, `last_end_tick`
- `last_exec_ticks`, `max_exec_ticks`, `total_exec_ticks`

### Trace hook API (enabled only with `SCH_ENABLE_TRACE=1`)

Register hook with:

- `sch_set_trace_hook(&scheduler, hook, user_ctx);`

Callback contract:

- runs synchronously inside `sch_run()`,
- must be non-blocking and bounded,
- must not call scheduler APIs reentrantly.

### Performance boundaries

- With both gates disabled, no instrumentation buffers, counters, or callback
  branches are present in the compiled scheduler hot path.
- With stats enabled, the scheduler performs bounded per-execution tick reads
  and counter updates.
- With tracing enabled, each emitted event is one guarded callback dispatch.

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

## Static analysis (Cppcheck)

The Docker image for Cppcheck is defined in `docker/cppcheck/Dockerfile` and runs `cppcheck` as its entrypoint.

The checks are configured by the Docker/local runner scripts:

- `--enable=all` (runs the full set of Cppcheck checks, matching the check catalog on the Cppcheck wiki)
- `--inconclusive`
- `--std=c11`
- `--inline-suppr`
- `-I include`
- `--suppress=missingIncludeSystem`
- Targets: `src include examples tests`
- Text report run also uses: `--error-exitcode=1`
- XML report run also uses: `--xml --xml-version=2`

You can configure these checks by editing the argument lists in:

- `scripts/run_cppcheck_docker.sh`
- `scripts/run_cppcheck_docker.ps1`
- `scripts/run_cppcheck_local.sh`
- `scripts/run_cppcheck_local.ps1`

### Which Cppcheck version is used?

The Dockerfile now builds and installs **Cppcheck 2.15.0** from the upstream `danmar/cppcheck` Git tag (`2.15.0`), so the containerized runner uses that exact version.

To verify in your built image:

```bash
docker build -t mialib-cppcheck:latest -f docker/cppcheck/Dockerfile .
docker run --rm mialib-cppcheck:latest --version
```
## Test coverage (Ceedling + gcov)

`gcov` is the GNU code coverage toolchain. It works by compiling test binaries with coverage instrumentation, then collecting execution data from your test run to report which lines and branches were exercised.

In this project, Ceedling uses its `gcov` plugin to automate that workflow (instrument build, run tests, collect coverage files, and generate reports).

Run coverage with:

```bash
ceedling gcov:all
```

Why `gcov:all` (instead of `test:all`)?

- `test:all` only builds and executes unit tests.
- `gcov:all` executes unit tests **with coverage instrumentation enabled** and produces coverage reports.

After running `gcov:all`, coverage reports are generated under `build/ceedling/artifacts/gcov/` (text + HTML).

## CMake vs Ceedling (important)

- **Keep building the project with CMake exactly as before.** No `CMakeLists.txt` change is required to use Ceedling coverage.
- **Ceedling is a separate test build system**: it compiles the unit-test runner(s) plus the production source files listed in `project.yml` (`src/`, test files, includes).
- `ceedling test:all` compiles and runs tests only.
- `ceedling gcov:all` compiles tests with coverage flags and then runs them to produce coverage reports.

So yes, **Ceedling does compile files** (its own test build), independently from your normal CMake build.
