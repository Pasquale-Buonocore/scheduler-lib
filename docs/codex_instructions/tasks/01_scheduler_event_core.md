# Task 1 — Hybrid Polling-First Event Semantics in Scheduler Core

> **Artifact classification:** design/backlog artifact (task prompt).
>
> **Status:** Implemented baseline in current repository unless explicitly marked as planned in the task body.
>
> For current behavior and skeleton limitations, `README.md` is authoritative.

## Purpose of this task file
Use this file as a **self-contained AI prompt spec** for implementing hybrid polling-first semantics in the scheduler core. It defines scope, constraints, expected outputs, and acceptance criteria so an AI can execute the task with minimal ambiguity.

## Implementation goal
Adopt a hybrid model where:
- service tasks are periodic and always enabled, and
- ISRs only capture data and optionally set lightweight hint flags.

## Why this matters
- Preserves a simple cooperative model (no ISR-triggered task activation/deactivation path required).
- Keeps ISR execution bounded and minimal.
- Maintains deterministic latency tied to polling period while enabling future optimization hooks.

## Required repository/context inputs for the AI
When using this file as a prompt, also provide:
1. Current scheduler core files (task model, dispatch loop, task state handling).
2. Existing event/hint abstractions.
3. Any ISR integration points currently toggling task execution state.
4. Build/test commands used in this repository.

## Non-negotiable constraints
- No dynamic memory allocation.
- ISR path must remain bounded and minimal.
- Periodic scheduling behavior must remain intact unless explicitly required.
- Do not introduce ISR-driven task enable/disable semantics.

## Design requirements
1. **Execution model**
   - Service tasks run at configured periods.
   - When no data exists, service tasks return quickly with minimal work.
2. **ISR contract**
   - ISR acknowledges hardware.
   - ISR captures minimal payload into SPSC buffer/queue.
   - ISR may set advisory hint bit/flag.
   - ISR exits without scheduler task-state mutation.
3. **Task contract**
   - Task checks buffer each scheduled run.
   - Task processes bounded quota per run.
   - Remaining backlog is deferred to future periodic runs.
4. **Hint/event semantics**
   - Hint bits are advisory, not counted.
   - Payload multiplicity/counting is represented only by queued data.

## Expected AI deliverables
- Code changes implementing/aligning scheduler semantics to the model above.
- Inline documentation/comments explaining ISR vs service-task responsibilities.
- Tests updated/added to prove periodic behavior and hint independence.
- Short implementation summary describing tradeoffs and latency implications.

## Suggested prompt template
```md
Implement Task 1 from `docs/codex_instructions/tasks/01_scheduler_event_core.md`.

Use the file as the source of truth. Ensure:
- periodic service tasks remain always enabled,
- ISR does not activate/deactivate tasks,
- hints are advisory only,
- bounded per-run task workload.

Return:
1) patch,
2) tests run + results,
3) concise rationale mapped to acceptance criteria.
```

## Acceptance criteria (definition of done)
- Periodic scheduling behavior remains unchanged.
- Service task correctness does not depend on hint/event presence.
- ISR time is bounded and task per-run work is bounded.
- No dynamic memory introduced.
- Tests validate new/updated behavior.
