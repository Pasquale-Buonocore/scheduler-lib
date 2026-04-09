# Task 4 — Hybrid Polling-First Example Application

> **Artifact classification:** design/backlog artifact (task prompt).
>
> **Status:** Implemented baseline in current repository unless explicitly marked as planned in the task body.
>
> For current behavior and skeleton limitations, `README.md` is authoritative.

## Purpose of this task file
Use this file as a **prompt blueprint** for creating a runnable example that demonstrates the hybrid polling-first architecture end-to-end.

## Implementation goal
Provide an example with:
- periodic control tasks,
- periodic polling service tasks,
- one background task,
all integrated into one coherent superloop.

## Why this matters
- Demonstrates the chosen architecture directly.
- Provides executable reference for ISR→buffer→task processing flow.

## Required repository/context inputs for the AI
When using this file as a prompt, provide:
1. Existing example/demo directory conventions.
2. Scheduler startup/registration APIs.
3. Buffer/event utility APIs available in repo.
4. Host-simulation utilities (if any) for ISR simulation.

## Non-negotiable constraints
- Example must compile in the current build setup.
- Must not rely on event-driven task wake-up for correctness.
- Service tasks must remain periodic and always eligible to run.
- Simulated ISR must not toggle task enable/disable state.

## Design requirements
1. **Example composition**
   - Fast periodic control task (e.g., 1 ms).
   - One or more periodic service tasks (UART/ETH style).
   - One background diagnostics/maintenance task.
2. **ISR simulation (host-safe)**
   - Simulated ISR pushes payload into ring buffer.
   - Simulated ISR may set advisory hint bit/flag.
   - No task state toggling.
3. **Service task behavior**
   - Quick empty check and immediate return.
   - Bounded max-items-per-run processing.
4. **Observability in demo output**
   - Processed item count,
   - empty polls,
   - overflow/drop counters.

## Expected AI deliverables
- Example source files and integration wiring.
- Clear README or inline usage notes explaining how to run.
- Demonstration output/log showing periodic polling behavior.
- Minimal tests (if applicable) to verify expected flow.

## Suggested prompt template
```md
Implement Task 4 from `docs/codex_instructions/tasks/04_main_events_example.md`.

Build a runnable polling-first example showing control, service, and background tasks.

Return:
1) patch,
2) run instructions,
3) sample output,
4) checks/tests run.
```

## Acceptance criteria (definition of done)
- Example compiles and runs in current repo setup.
- Demonstrates periodic polling behavior with optional hints.
- Correctness does not depend on event-driven task wake-up.
- Shows bounded per-run processing and useful counters.
