# Cooperative Scheduler — Missing Pieces: Events, ISR Bridge, Drivers, and System Integration (Codex Prompt)

This document is a **detailed implementation prompt** for Codex (or similar) to extend an existing **C99 cooperative bare-metal scheduler** (already implemented for periodic/background tasks) with the missing pieces needed for a real embedded system:

- **Event mechanism**
- **ISR-to-task bridging**
- **Non-blocking driver/service patterns** for UART / Ethernet / I2C (EEPROM) / ADC-PWM (motor control integration)
- **Instrumentation hooks** (overrun, missed deadlines, tracing)
- **Integration guidance** for multi-platform targets (C28x, ARM R5F, ARM A53)

The end goal is an **architecture that remains cooperative** while using **interrupts only for event capture**, and executing the actual work in scheduled tasks.

---

## 0. Context and Constraints

### Existing Library Baseline (already implemented)
Assume there is an existing scheduler library with:
- static task array, up to 64 tasks
- periodic tasks (`period_ticks > 0`), background tasks (`period_ticks == 0`)
- priority-based dispatch ordering (no preemption)
- `pending` flag marking “ready-to-run” tasks
- time base is **free-running** and converted to **1 tick = 1 µs**
- overflow-safe time comparisons

### Language/Environment
- C99 only
- no dynamic memory allocation
- no recursion
- MISRA-friendly style preferred (but not strict MISRA compliance required)
- keep ISRs short; avoid heavy work in interrupt context

### Port Layer
Assume port functions exist:
```c
uint32_t sch_port_now_ticks(void);              // time in microseconds (wraparound OK)
uint32_t sch_port_enter_critical(void);         // save+disable IRQ
void     sch_port_exit_critical(uint32_t state);// restore IRQ state
void     sch_port_idle(void);                   // NOP/WFI
```

---

## 1. Why Events and an ISR Bridge are Needed

A cooperative scheduler alone (periodic polling) can work for low-rate, low-burst systems.

But for systems with:
- Ethernet RX bursts
- UART RX bursts
- I2C completion events
- sensor “data ready” interrupts
- fault interrupts

pure polling either wastes CPU or risks data loss due to FIFO overflow.

Therefore the design must support **event-driven activation** of tasks, while keeping the scheduler cooperative.

**Key rule:**  
> ISR must only ACK hardware, move minimal data to buffers, set events, and exit.  
> All processing happens in tasks.

---

## 2. Event System Requirements

### 2.1 Event Representation
Implement a **global event mask** (32 or 64-bit). Prefer 32-bit for simplicity and atomicity.

Example:
```c
typedef uint32_t sch_event_mask_t;

#define EVT_UART_RX      (1u << 0)
#define EVT_UART_TX      (1u << 1)
#define EVT_ETH_RX       (1u << 2)
#define EVT_ETH_TX       (1u << 3)
#define EVT_I2C_DONE     (1u << 4)
#define EVT_I2C_ERR      (1u << 5)
#define EVT_ADC_EOC      (1u << 6)
#define EVT_FAULT        (1u << 7)
```

### 2.2 ISR-Safe Event Set
Provide a function callable from ISRs:
```c
void sch_event_set_isr(sch_t *s, sch_event_mask_t mask);
```

Implementation rules:
- must be O(1)
- must be interrupt-safe
- should not call `sch_port_enter_critical()` if the ISR is already in an IRQ-disabled context; however for portability you can still use critical sections if they are “save/disable” based.
- the simplest safe approach: use `sch_port_enter_critical()` / `sch_port_exit_critical()` around a bitwise OR on a volatile mask.

### 2.3 Event Consumption in Scheduler
In `sch_run()`:
- snapshot events and clear them
- translate events into `pending = true` for the tasks that subscribe to those events

Design options:

**Option A (recommended): per-task subscription mask**
- each task stores `event_subscribe_mask`
- if `(events & task.mask) != 0` => task.pending = true

This is scalable and avoids fixed mapping tables.

### 2.4 Event API
Add APIs:
```c
void sch_event_subscribe(sch_t *s, uint32_t task_id, sch_event_mask_t mask);
sch_event_mask_t sch_event_get_and_clear(sch_t *s);
```

But prefer internal consumption in sch_run() (get+clear is internal).

### 2.5 Event Semantics
- Events are **level-less** “edge notifications” (bit is set, then cleared on consumption).
- If multiple occurrences happen before consumption, they collapse into one event bit.
- For counted events (e.g., multiple RX frames), use ring buffers/queues for data, not event bit multiplicity.

---

## 3. ISR-to-Task Bridging Patterns

### 3.1 Ring Buffers for Data
For UART and other stream-like interfaces:
- ISR pushes bytes into a ring buffer (SPSC style)
- task drains the buffer

Requirements:
- SPSC ring buffer (single producer ISR, single consumer main)
- power-of-two sizing recommended (masking), but allow non-power-of-two if needed
- must be lock-free or protected with minimal critical section

Provide a small reusable ring buffer implementation:
- `rb_init`
- `rb_push_isr`
- `rb_pop`
- `rb_available`
- optional overflow counter

### 3.2 Descriptor/Queue for Ethernet
For Ethernet with DMA descriptors:
- ISR should only:
  - acknowledge interrupt
  - push descriptor indices into a queue OR set a flag that RX descriptors are ready
  - set `EVT_ETH_RX`
- Task (`Eth_Service`) should:
  - drain descriptors in bounded batches
  - copy/parse frames as needed
  - return quickly

### 3.3 Transaction State Machines for I2C/SPI
For EEPROM writes/reads:
- avoid blocking I2C calls inside tasks
- implement non-blocking transaction state machine:
  - task initiates transfer
  - ISR sets `EVT_I2C_DONE` or `EVT_I2C_ERR`
  - task continues next step upon event

---

## 4. Motor Control / ADC-PWM Integration Guidance

This scheduler is cooperative and is not intended to replace hard real-time motor loops.

Define two “timing domains”:

### 4.1 Fast Control Domain (ISR Driven)
- PWM trigger + ADC sample + current loop update often must remain in an ISR
- Keep it small and deterministic
- Optional: produce outputs into shared structs/ring buffers

### 4.2 Supervisor Domain (Scheduler Driven)
Use scheduler tasks for:
- setpoint management
- state machine (IDLE/RUN/FAULT)
- safety monitoring
- communications integration (commands)
- logging/telemetry

Bridge from ISR to supervisor using:
- event bits (e.g., `EVT_ADC_EOC`) only if needed
- shared “latest sample” variables with volatile + critical section or double-buffering

---

## 5. Instrumentation Hooks (Strongly Recommended)

Even without preemption, the system must detect overload and jitter risk.

Add optional hooks:

### 5.1 Execution Time Measurement Hook
Allow port layer to provide an optional cycle counter:
```c
uint32_t sch_port_cycles_now(void);
uint32_t sch_port_cycles_to_us(uint32_t cycles_delta);
```

If not available, compile out.

### 5.2 Per-Task Stats
For each task:
- `last_start_us`
- `last_exec_us`
- `max_exec_us`
- `deadline_miss_count`
- `overrun_count` (if exec > period, or if release was missed)

### 5.3 Trace Callbacks
Allow user to register callbacks:
```c
typedef void (*sch_trace_fn_t)(uint32_t task_id, uint32_t now_us);

void sch_set_trace_on_start(sch_trace_fn_t fn);
void sch_set_trace_on_end(sch_trace_fn_t fn);
void sch_set_trace_on_miss(sch_trace_fn_t fn);
```

Keep callbacks optional and compiled out by macro if desired.

---

## 6. Task Design Rules (Document and Enforce Where Possible)

Tasks must:
- be non-blocking
- do bounded work per activation
- return quickly
- never busy-wait on hardware completion

For comms tasks, implement “bounded service”:
- process up to `N` items per call (bytes, frames, transactions)
- or process until a time budget is reached (future enhancement)

---

## 7. Deliverables to Implement

Codex must create or modify files to deliver these features.

### 7.1 New/Updated Scheduler Files
- Update existing `scheduler.h` / `scheduler.c` to add:
  - event mask in scheduler state
  - per-task event subscription mask
  - ISR-safe event set API
  - event consumption in `sch_run()` before selecting ready tasks
  - (optional) instrumentation + trace hooks behind macros

### 7.2 New Utility Modules
Create reusable modules in `utils/`:

- `ring_buffer.h/.c`
  - SPSC ring buffer for UART or generic byte streams

- `event_queue.h/.c` (optional)
  - If needed for counted events (descriptor indices, etc.)
  - Otherwise descriptor queues can reuse ring buffer with element size

### 7.3 Driver/Service Skeletons (portable architecture)
Create skeleton code (interfaces only) for:
- `uart_driver.h/.c` (ISR pushes to RX rb, sets EVT_UART_RX)
- `uart_service.h/.c` (task drains rb, parses messages, enqueues TX)

- `eth_driver.h/.c` (ISR signals RX descriptor ready, sets EVT_ETH_RX)
- `eth_service.h/.c` (task drains descriptors, bounded batch processing)

- `i2c_driver.h/.c` (non-blocking API + ISR completion event)
- `eeprom_service.h/.c` (transaction state machine)

- `motor_supervisor.h/.c` (scheduler task, state machine)
- `motor_fast_isr.h/.c` (placeholder, ISR-only control loop)

These should be written as **platform-agnostic** layers that depend on a HAL to actually touch registers.

### 7.4 Example Application
Create `examples/main_events_example.c` showing:
- periodic tasks: 1ms, 10ms
- event tasks: UART_Service, ETH_Service, EEPROM_Service
- background task: Diagnostics
- simulated event set calls (or stubs) if hardware not present

---

## 8. Recommended Directory Structure

```
scheduler/
├── scheduler.h
├── scheduler.c
├── port/
│   └── scheduler_port.h
├── utils/
│   ├── ring_buffer.h
│   ├── ring_buffer.c
│   ├── event_queue.h        (optional)
│   └── event_queue.c        (optional)
├── services/
│   ├── uart_service.h
│   ├── uart_service.c
│   ├── eth_service.h
│   ├── eth_service.c
│   ├── eeprom_service.h
│   ├── eeprom_service.c
│   ├── motor_supervisor.h
│   └── motor_supervisor.c
├── drivers/
│   ├── uart_driver.h
│   ├── uart_driver.c
│   ├── eth_driver.h
│   ├── eth_driver.c
│   ├── i2c_driver.h
│   └── i2c_driver.c
└── examples/
    ├── main_example.c
    └── main_events_example.c
```

---

## 9. Detailed TODO List (What’s Missing)

### Scheduler Core
- [ ] Add scheduler-level `volatile sch_event_mask_t events;`
- [ ] Add per-task `sch_event_mask_t subscribe_mask;`
- [ ] Implement `sch_event_set_isr(sch_t*, mask)`
- [ ] Implement event subscription API `sch_event_subscribe()`
- [ ] In `sch_run()`, consume events and set `pending` for subscribed tasks
- [ ] Ensure critical sections protect event mask and `pending` modifications
- [ ] Document event semantics (bit-collapsing, use buffers for multiplicity)

### Utilities
- [ ] Implement SPSC ring buffer module (ISR producer, task consumer)
- [ ] Add overflow counter and optional drop policy
- [ ] (Optional) Implement generic index queue for DMA descriptors

### UART
- [ ] Define UART driver ISR pattern: drain FIFO → push to rb → `EVT_UART_RX`
- [ ] Define UART service task: bounded drain → parse → enqueue TX
- [ ] Decide TX strategy: TX-empty ISR drains TX rb OR periodic “TX kick”

### Ethernet
- [ ] Define ETH driver ISR: ACK + set EVT_ETH_RX/EVT_ETH_TX; minimal descriptor work
- [ ] Define ETH service task: bounded RX batch processing, descriptor recycling
- [ ] Provide clear separation between HAL (registers) and service logic

### I2C / EEPROM
- [ ] Non-blocking I2C driver interface (start transfer, check busy)
- [ ] ISR completion sets EVT_I2C_DONE/EVT_I2C_ERR
- [ ] EEPROM service implements transaction state machine (page writes, polling busy, retries)
- [ ] Bounded work per activation (no long loops)

### Motor Control Integration
- [ ] Define “fast ISR domain” placeholder (PWM/ADC ISR)
- [ ] Provide supervisor task for state machine and safety checks
- [ ] Define shared-data handoff pattern (double-buffering or critical-protected copies)

### Instrumentation (Optional but Recommended)
- [ ] Add per-task timing stats (last/max exec time)
- [ ] Add missed-deadline detection (release late)
- [ ] Add trace callback hooks (start/end/miss)
- [ ] Compile-time macros to enable/disable stats to avoid overhead

### Examples and Tests
- [ ] Provide `main_events_example.c`
- [ ] Provide host-side unit-test stubs (fake time + fake ISR event set) (optional)
- [ ] Provide “GPIO toggle hook points” comments for hardware timing measurement

---

## 10. Implementation Notes and Non-Goals

### Non-goals for this step
- full RTOS features
- dynamic priority or preemptive scheduling
- blocking driver APIs

### Key correctness notes
- Events are **notifications**, not data containers.
- Data must be buffered (ring buffers / descriptor queues).
- Always keep ISR bounded and short.
- Keep tasks bounded (batch size / no indefinite loops).

---

## 11. Acceptance Criteria

Implementation is accepted if:
- scheduler compiles as C99
- periodic scheduling still works unchanged
- event-driven tasks can be woken via `sch_event_set_isr`
- UART/Ethernet/I2C skeletons demonstrate correct ISR-to-task bridging patterns
- examples compile and show usage patterns clearly
- code is readable and portable (HAL boundaries respected)
