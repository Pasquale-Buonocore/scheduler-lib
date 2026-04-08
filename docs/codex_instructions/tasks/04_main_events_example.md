# Task 4 — Hybrid Polling-First Example Application

## Goal
Provide an example showing periodic control tasks, periodic polling service tasks, and a background task in one coherent superloop.

## Why this matters
- Demonstrates the chosen hybrid architecture directly.
- Provides an executable reference for ISR→buffer→task processing flow.

## Design discussion points
1. Example composition:
   - fast periodic control task (e.g., 1 ms),
   - one or more periodic service tasks (UART/ETH style),
   - one background diagnostics task.
2. ISR simulation on host:
   - simulated ISR pushes payload into ring buffer,
   - optionally sets hint bit/flag,
   - does not toggle task enable/disable state.
3. Service-task behavior:
   - quick empty check and immediate return,
   - bounded max-items-per-run processing.
4. Observability:
   - show processed item count, empty polls, and overflow/drop counters.

## Acceptance notes
- Example compiles in current repo build setup.
- Clearly demonstrates periodic polling behavior plus optional hint usage.
- No event-driven task wake-up dependency.
