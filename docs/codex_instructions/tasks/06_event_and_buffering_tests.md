# Task 6 — Polling-First Scheduler and Buffering Test Expansion

## Goal
Extend unit tests to validate hybrid polling semantics and ISR/task buffering correctness.

## Why this matters
- Prevents regressions in concurrency-adjacent paths.
- Confirms correctness under periodic polling cadence and backlog conditions.

## Design discussion points
1. Scheduler behavior tests:
   - service task runs on configured period regardless of hint/event,
   - empty-buffer path returns without side effects,
   - queued data is processed on subsequent periodic runs.
2. Latency-bound tests:
   - verify processing occurs within expected bound from task period/scheduler order.
3. Bounded-work tests:
   - enforce max-items-per-run behavior and deferred backlog handling.
4. Buffer tests:
   - ordering, overflow counters, empty/full behavior, ISR-producer/task-consumer assumptions.
5. Hint/event tests:
   - hint is advisory; processing correctness relies on buffer contents.

## Acceptance notes
- New tests are deterministic and host-runnable.
- Existing periodic/background tests continue to pass.
- No tests assume ISR-driven task activation.
