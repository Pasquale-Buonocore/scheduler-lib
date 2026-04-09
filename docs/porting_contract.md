# Scheduler porting contract

This document defines the required platform semantics for the port hooks in
`include/scheduler/port/scheduler_port.h`.

## Scope

The scheduler and ISR/task buffering utilities assume that:

- shared scheduler metadata updates are atomic with respect to ISR preemption,
- enter/exit critical calls are correctly paired and restore prior interrupt state,
- ISR-produced data is visible to task context after leaving critical sections.

If your target cannot satisfy these rules, scheduler behavior is undefined.

## Required semantics

### `sch_port_enter_critical`

`uint32_t sch_port_enter_critical(void)` must:

1. Prevent scheduler-shared state from being concurrently modified by ISR code.
   - On single-core MCUs this typically means raising interrupt mask / disabling
     preempting IRQ levels.
2. Return a token that fully captures the previous interrupt/critical state for
   later restoration by `sch_port_exit_critical`.
3. Support nesting: each call must be independently restorable in strict LIFO
   order.
4. Execute in bounded, deterministic time (constant-time register operations,
   no loops waiting on external conditions).

### `sch_port_exit_critical`

`void sch_port_exit_critical(uint32_t state)` must:

1. Restore the exact pre-entry state represented by `state`.
   - Do not unconditionally enable interrupts.
2. Preserve nesting correctness.
   - Exiting an inner critical section must not enable interrupts if an outer
     section is still active.
3. Provide release ordering for shared memory updates made in the critical
   section before interrupts/other observers can run.

## ISR call-context assumptions

Scheduler helpers used in ISR paths (for example
`sch_spsc_ring_push_isr` / `sch_event_queue_push_isr`) assume:

- ISR code can call the same critical-section API safely.
- Entering/exiting from ISR context is either a no-op or maps to an interrupt
  priority masking model that still protects shared data.
- ISR path remains bounded and non-blocking.
- ISR code never calls `sch_run` (scheduler execution is task/thread context).

For priority-based controllers (for example ARM GIC/R5F/A53 style designs),
critical sections must block any ISR priority that can touch scheduler-shared
structures. Masking only lower-priority interrupts is insufficient if higher
priorities can also access scheduler state.

## Nesting behavior (must-have)

The implementation must be reentrant with respect to nested critical sections:

- Multiple `enter` calls from the same execution context are allowed.
- Each matching `exit` only undoes one level.
- Final outermost `exit` restores exactly the prior interrupt mask.

A common pattern is to return status-register snapshots (`PRIMASK`, `BASEPRI`,
C28x `INTM`, etc.) and restore that snapshot in `exit`.

## Memory ordering and barriers

At minimum, the port must ensure:

- **Acquire-like effect on enter**: subsequent loads/stores in the critical
  section observe a consistent view of shared scheduler data.
- **Release-like effect on exit**: updates inside the critical section become
  visible before interrupts resume.

On strongly ordered MCUs, interrupt mask changes may already provide enough
ordering. On weaker-ordering or multicore systems (A53-class), add explicit
compiler/CPU barriers (`dmb ish`, equivalent intrinsics, or toolchain-provided
fences) around critical boundaries as needed.

## Incorrect implementation example

### Broken pattern

```c
uint32_t sch_port_enter_critical(void) {
    __disable_irq();
    return 0u; /* ignores previous state */
}

void sch_port_exit_critical(uint32_t state) {
    (void)state;
    __enable_irq(); /* always enables */
}
```

### Why this breaks correctness

- If called inside an already-masked region, inner `exit` re-enables interrupts
  too early (nesting violation).
- ISR can interleave halfway through scheduler metadata updates, causing torn
  ready flags / ring index corruption.
- On weakly ordered systems, writes may not be visible before interrupt resume
  without proper ordering.

Result: missed wakeups, duplicated/dropped ring entries, or periodic task state
becoming inconsistent across ISR/task contexts.

## Board bring-up validation checklist

Use this quick checklist on new targets:

1. **Tick monotonicity**
   - Verify `sch_port_now_ticks()` never goes backwards for scheduler-observed
     samples (wrap-around is acceptable modulo `uint32_t`).
2. **Critical nesting correctness**
   - Add a test that enters twice, exits once, and confirms interrupts are still
     masked until the second exit.
3. **ISR/task race safety**
   - Stress ISR push + task pop on rings/queues; confirm no count corruption,
     no impossible sizes, and expected drop accounting.
4. **Idle safety**
   - Verify `sch_port_idle()` does not violate wakeup behavior (e.g. WFI with
     proper interrupt enable state and no deadlock).
5. **Memory visibility**
   - On weakly ordered/multicore targets, run stress tests with barriers
     enabled and confirm deterministic ISR-to-task handoff.
