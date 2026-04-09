
# Cooperative Baremetal Scheduler — Codex Development Prompt

> **Artifact classification:** design/backlog artifact (planning prompt).
>
> **Status labels:**
> - **Implemented (current repo):** cooperative periodic/background scheduler core, static task model, bounded run loop, compatibility no-op `sch_enable_task()`, and optional stats/trace gates.
> - **Planned (prompt-only):** any behavior in this file that conflicts with README current behavior.
>
> For current behavior and current skeleton limitations, use `README.md` as the source of truth.

This document contains:
1. **Complete technical specification** for a portable cooperative scheduler library.
2. **An optimized prompt** designed for OpenAI Codex or similar code-generation systems to implement the library correctly.

The specification targets **embedded bare‑metal systems** and prioritizes deterministic behavior, portability, and simplicity.

---

# PART 1 — Scheduler Technical Specification

## Purpose

Design and implement a **portable cooperative scheduler library in C (C11)** intended for **baremetal embedded systems**.

The scheduler must be:

- deterministic
- portable across CPU architectures
- minimal
- independent of RTOS
- safe for embedded applications

Target platforms:

- TI **C28x**
- ARM **R5F**
- ARM **A53**

The first version must support **periodic tasks only** but must be designed so that **event-driven tasks can be added later**.

---

# Scheduler Architecture

The scheduler is **cooperative**.

Properties:

- Tasks run **to completion**
- No **preemption**
- Tasks must be **short**
- Tasks must **not block**
- Tasks must return control to the scheduler

Typical application loop:

```c
while (1)
{
    sch_run(&scheduler);
}
```

The scheduler decides which tasks must run based on time.

---

# Time Base

Scheduler time unit:

```
1 tick = 1 microsecond (1 µs)
```

The underlying hardware timer **does not need to run at 1 MHz**.

Instead the platform layer converts the hardware counter to microseconds.

Example:

```
Hardware timer = 100 MHz
ticks_us = counter / 100
```

Scheduler timestamps use:

```
uint32_t time_us
```

Overflow must be supported.

Safe comparison rule:

```c
(int32_t)(now - target) >= 0
```

Overflow period with 32‑bit microsecond counter:

```
~71 minutes
```

This is acceptable.

---

# Portability Layer

The scheduler **must not access hardware directly**.

Instead it relies on a **port layer** implemented per platform.

Required functions in `scheduler_port.h`.

### Time

```c
uint32_t sch_port_now_ticks(void);
```

Returns current time in microseconds.

---

### Critical Sections

```c
uint32_t sch_port_enter_critical(void);
void sch_port_exit_critical(uint32_t state);
```

Typical implementation:

- disable interrupts
- store previous interrupt state
- restore interrupt state

---

### Idle Hook

```c
void sch_port_idle(void);
```

Executed when the scheduler has nothing to run.

Typical implementations:

```
NOP
WFI
```

---

# Maximum Tasks

Maximum supported tasks:

```
64
```

Defined with macro:

```c
#define SCH_MAX_TASKS 64
```

Tasks must be **statically allocated**.

Dynamic memory is **not allowed**.

---

# Task Model

Each task contains:

```
function pointer
context pointer
period
next_release time
priority
enabled flag
pending flag
```

Tasks are stored in a static array.

---

## Task Function

```c
typedef void (*sch_task_fn_t)(void *ctx);
```

---

# Periodic Tasks

Periodic tasks have:

```
period_ticks > 0
```

Examples:

```
1 ms  -> period = 1000
10 ms -> period = 10000
```

---

# Background Tasks

A task is **background** if:

```
period_ticks == 0
```

Background tasks:

- run only when no periodic tasks are ready
- have no timing guarantees

Typical uses:

- diagnostics
- logging
- housekeeping

---

# next_release

`next_release` represents the **absolute time when the task should run next**.

Example:

```
period = 10000 us
next_release = 1000000 us
```

A task becomes ready when:

```
now >= next_release
```

After execution:

```
next_release += period
```

If periods were missed:

```
while(next_release <= now)
    next_release += period;
```

---

# Pending Flag

`pending` indicates that a task is **ready to execute**.

Reasons for this flag:

- manage scheduler logic
- enforce deterministic execution
- support future event-driven tasks
- prevent duplicate execution

---

# Priority

Priority determines **execution order only**.

No preemption exists.

Range:

```
0 = highest priority
255 = lowest priority
```

Example:

| Task | Priority |
|-----|-----|
UART Service | 0 |
SPI Service | 1 |
Application | 10 |
Diagnostics | 20 |

---

# Scheduler Execution Policy

Each scheduler cycle:

1. Read current time
2. Mark periodic tasks as pending
3. Select ready tasks ordered by priority
4. Execute them sequentially
5. Update `next_release`
6. Clear `pending`
7. Repeat until no tasks are ready
8. If none are ready, run one background task
9. If no background tasks exist, call `sch_port_idle()`

---

# Ready Rule

A task is ready when:

```c
(int32_t)(now - next_release) >= 0
```

---

# API

## Initialization

```c
void sch_init(sch_t *scheduler);
```

---

## Add Task

```c
int32_t sch_add_task(
    sch_t *scheduler,
    sch_task_fn_t fn,
    void *ctx,
    uint32_t period_ticks,
    uint32_t start_delay_ticks,
    uint8_t priority
);
```

Return:

```
>=0 task id
<0 error
```

---

## Enable / Disable

```c
void sch_enable_task(
    sch_t *scheduler,
    uint32_t task_id,
    bool enable
);
```

---

## Run Scheduler

```c
void sch_run(sch_t *scheduler);
```

Runs one scheduler iteration.

Must:

- execute ready tasks
- execute one background task if available
- never block

---

# File Structure

```
scheduler/
│
├── scheduler.h
├── scheduler.c
│
├── port/
│   └── scheduler_port.h
│
└── examples/
    └── main_example.c
```

---

# PART 2 — Optimized Prompt for Codex

Use the following prompt when asking Codex to implement the scheduler.

---

## Codex Prompt

You are implementing a **portable cooperative scheduler library in C (C11)** for bare‑metal embedded systems.

Your implementation must follow these constraints:

### Architecture

The scheduler must:

- support up to **64 tasks**
- use **static allocation only**
- support **periodic tasks**
- support **background tasks**
- run in **cooperative mode**
- avoid dynamic memory
- avoid recursion
- avoid blocking calls

### Timing

Scheduler tick unit:

```
1 tick = 1 microsecond
```

The scheduler must read time using:

```
uint32_t sch_port_now_ticks(void)
```

Use **overflow-safe comparison**:

```c
(int32_t)(now - deadline) >= 0
```

### Tasks

Each task must contain:

- function pointer
- context pointer
- period
- next_release timestamp
- priority
- enabled flag
- pending flag

### Scheduling Algorithm

Each call to `sch_run()` must:

1. read current time
2. mark periodic tasks ready
3. execute ready tasks ordered by priority
4. update next_release
5. clear pending
6. continue until no tasks are ready
7. execute one background task if available
8. otherwise call `sch_port_idle()`

### Port Layer

Do not implement hardware code.

Assume these functions exist:

```
uint32_t sch_port_now_ticks(void);
uint32_t sch_port_enter_critical(void);
void sch_port_exit_critical(uint32_t);
void sch_port_idle(void);
```

### Deliverables

Generate:

```
scheduler.h
scheduler.c
scheduler_port.h
example main.c
```

### Code Quality

The code must:

- compile with **C11**
- avoid undefined behaviour
- be readable
- contain comments
- be suitable for embedded systems
