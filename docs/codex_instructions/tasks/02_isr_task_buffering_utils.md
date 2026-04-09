# Task 2 — ISR→Task Buffering Utilities

> **Artifact classification:** design/backlog artifact (task prompt).
>
> **Status:** Implemented baseline in current repository unless explicitly marked as planned in the task body.
>
> For current behavior and skeleton limitations, `README.md` is authoritative.

## Purpose of this task file
Use this file as a **direct AI prompt contract** for building reusable ISR-producer/task-consumer buffering utilities.

## Implementation goal
Create reusable utility modules for transferring data captured in ISRs to scheduler tasks.

## Why this matters
- Event bits should indicate "work exists", not carry payload.
- Stream/burst data (UART RX, descriptor indices) requires queues/buffers.

## Required repository/context inputs for the AI
When prompting with this file, include:
1. Existing utility/container modules (if any).
2. Current scheduler integration points for ISR and service tasks.
3. Existing tests style/framework.
4. Target constraints (embedded-safe, allocation policy, performance expectations).

## Non-negotiable constraints
- No dynamic memory allocation.
- Deterministic bounded operations for push/pop.
- Clear ownership: ISR producer, task consumer (SPSC assumption).
- Overflow handling behavior must be explicit and test-covered.

## Design requirements
1. **SPSC ring buffer API and ownership model**
   - Provide minimal, ergonomic API for ISR push and task pop.
   - Document thread/interrupt safety assumptions.
2. **Overflow behavior options**
   - drop newest,
   - drop oldest,
   - count drops.
   - Ensure behavior is explicit and selectable/documented.
3. **Capacity model**
   - Consider power-of-two optimization and/or generic sizing.
   - State complexity/performance tradeoffs.
4. **Optional event/index queue utility**
   - Support lightweight descriptor/event ID queueing where payload is external.

## Expected AI deliverables
- Buffer/queue utility implementation(s).
- API documentation for ISR/task usage model.
- Unit tests for ordering, empty/full behavior, overflow strategy, and drop counters.
- Integration guidance for scheduler service tasks.

## Suggested prompt template
```md
Implement Task 2 from `docs/codex_instructions/tasks/02_isr_task_buffering_utils.md`.

Follow all constraints in the file. Focus on deterministic ISR-producer/task-consumer utilities with tested overflow behavior.

Return:
1) patch,
2) API notes,
3) tests run + results,
4) any assumptions.
```

## Acceptance criteria (definition of done)
- ISR-producer/task-consumer safety assumptions are documented.
- Buffer operations are deterministic and bounded.
- Tests validate FIFO ordering and overflow behavior.
- Drop counters/observability are validated where applicable.
