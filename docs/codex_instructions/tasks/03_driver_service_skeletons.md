# Task 3 — Driver/Service Skeleton Architecture

> **Artifact classification:** design/backlog artifact (task prompt).
>
> **Status:** Implemented baseline in current repository unless explicitly marked as planned in the task body.
>
> For current behavior and skeleton limitations, `README.md` is authoritative.

## Purpose of this task file
Use this file as a **tasking prompt** for generating platform-agnostic driver/service skeletons that demonstrate ISR-to-task handoff patterns.

## Implementation goal
Add driver/service skeletons for:
- UART,
- Ethernet,
- I2C/EEPROM,
- motor control supervision.

## Why this matters
- Enforces architectural boundaries: HAL in drivers, business/state logic in services.
- Keeps ISRs short and deterministic.

## Required repository/context inputs for the AI
Include with this prompt:
1. Current module/file layout conventions.
2. Build system and include-path conventions.
3. Existing scheduler APIs and event/buffer primitives.
4. Any HAL abstraction interfaces already present.

## Non-negotiable constraints
- Skeletons must compile/link in current repository configuration.
- No direct target-specific register access in generic skeleton files.
- ISRs remain minimal and bounded.
- Service loops must process bounded work per run.

## Design requirements
1. **Minimal ISR contracts per peripheral**
   - Acknowledge source, capture minimal data/index, optional hint.
2. **Bounded batch processing in services**
   - Configurable max-items-per-run.
   - Fast return on empty path.
3. **Clear handoff primitives**
   - Event/hint bits are advisory.
   - Buffers/queues hold multiplicity/payload.
4. **HAL boundary clarity**
   - Drivers isolate hardware interaction.
   - Services operate on abstracted interfaces and state.

## Expected AI deliverables
- Skeleton source/header files for listed peripheral domains.
- Lightweight documentation comments showing extension points.
- Example wiring into scheduler/task registration where relevant.
- Build verification output.

## Suggested prompt template
```md
Implement Task 3 from `docs/codex_instructions/tasks/03_driver_service_skeletons.md`.

Create compile-ready, platform-agnostic driver/service skeletons for UART, Ethernet, I2C/EEPROM, and motor supervision.

Return:
1) patch,
2) architecture notes (ISR vs service responsibilities),
3) build/test results.
```

## Acceptance criteria (definition of done)
- Skeleton files compile and link in repo build setup.
- ISR responsibilities are minimal and deterministic.
- Service responsibilities show bounded periodic processing.
- HAL/service boundary is clear and extensible.
