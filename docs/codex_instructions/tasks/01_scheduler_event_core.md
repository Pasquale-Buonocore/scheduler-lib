# Task 1 — Hybrid Polling-First Event Semantics in Scheduler Core

## Goal
Adopt a hybrid model where service tasks are periodic and always enabled, while ISRs only capture data and optionally set lightweight hint flags.

## Why this matters
- Preserves a simple cooperative model (no ISR-triggered task activation/deactivation path required).
- Keeps ISR execution bounded and minimal.
- Maintains deterministic latency tied to polling period while enabling future optimization hooks.

## Design discussion points
1. **Execution model**
   - Service tasks run at configured periods and return quickly when no data exists.
   - No scheduler-level task activation/deactivation from ISR events.
2. **ISR contract**
   - ISR acknowledges hardware, captures minimal payload to SPSC buffer, optionally sets hint flag/bit, and exits.
3. **Task contract**
   - Task checks buffer each run, processes bounded work quota, defers remaining backlog to future runs.
4. **Hint/event semantics**
   - Hint bits are non-counted and advisory.
   - Payload multiplicity/counting is represented only by queued/buffered data.

## Acceptance notes
- Periodic scheduling behavior remains unchanged.
- Service task correctness does not depend on hint/event presence.
- Bounded ISR time and bounded per-run task work are documented.
- No dynamic memory.
