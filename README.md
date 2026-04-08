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
- `include/scheduler/port/scheduler_port.h`: platform/porting hooks.
- `src/scheduler.c`: scheduler implementation.
- `src/isr_buffer.c`: buffering utility implementation.
- `examples/main_example.c`: minimal integration example.
- `tests/unit/test_scheduler.c`: unit tests (Unity/Ceedling).

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

## Build (CMake)

```bash
cmake -S . -B build
cmake --build build
./build/main_example
```

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
