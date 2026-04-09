# Task 6 — Polling-First Scheduler and Buffering Test Expansion

> **Artifact classification:** design/backlog artifact (task prompt).
>
> **Status:** Implemented baseline in current repository unless explicitly marked as planned in the task body.
>
> For current behavior and skeleton limitations, `README.md` is authoritative.

## Purpose of this task file
Use this file as a **test-focused AI prompt** to expand host-runnable coverage for polling-first scheduler behavior and ISR/task buffering.

## Implementation goal
Extend unit tests to validate hybrid polling semantics and ISR/task buffering correctness.

## Why this matters
- Prevents regressions in concurrency-adjacent paths.
- Confirms correctness under periodic polling cadence and backlog conditions.

## Required repository/context inputs for the AI
Provide with this prompt:
1. Existing scheduler and buffer test suites.
2. Test framework conventions (fixtures, naming, assertions).
3. Determinism requirements for CI execution.
4. Build/test commands used by maintainers.

## Non-negotiable constraints
- Tests must be deterministic and host-runnable.
- Tests must not assume ISR-driven task activation.
- Existing periodic/background behavior tests must remain valid.
- Prefer narrowly scoped tests over broad/flaky timing assumptions.

## Test design requirements
1. **Scheduler behavior tests**
   - service task runs on configured period regardless of hint/event,
   - empty-buffer path returns without side effects,
   - queued data processes on subsequent periodic runs.
2. **Latency-bound tests**
   - verify processing within expected bound from task period and scheduler order.
3. **Bounded-work tests**
   - enforce max-items-per-run.
   - verify deferred backlog handling.
4. **Buffer tests**
   - FIFO ordering,
   - overflow/drop counters,
   - empty/full behavior,
   - ISR-producer/task-consumer assumptions.
5. **Hint/event tests**
   - hint is advisory only.
   - correctness depends on buffer contents.

## Expected AI deliverables
- New/updated tests grouped clearly by behavior.
- Any helper utilities needed to keep tests readable.
- Evidence of deterministic execution (no flaky timing dependence).
- Test run output summary.

## Suggested prompt template
```md
Implement Task 6 from `docs/codex_instructions/tasks/06_event_and_buffering_tests.md`.

Expand deterministic tests for polling-first scheduler and ISR/task buffering semantics.

Return:
1) patch,
2) list of added test scenarios,
3) test commands + results.
```

## Acceptance criteria (definition of done)
- New tests are deterministic and host-runnable.
- Existing periodic/background tests continue to pass.
- No tests rely on ISR-driven task activation semantics.
- Coverage demonstrates polling-first correctness and buffering robustness.
