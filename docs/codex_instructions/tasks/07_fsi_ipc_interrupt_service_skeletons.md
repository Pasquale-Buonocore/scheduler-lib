# Task 7 — FSI/IPC Interrupt Service Skeletons

## Purpose of this task file
Use this file as a **tasking prompt** for generating platform-agnostic FSI and IPC interrupt handling skeletons that follow the same ISR-to-task handoff model used in Task 3.

## Implementation goal
Add driver/service skeletons for:
- FSI (RX/TX events, frame handling, error handling),
- IPC (message notify/acknowledge paths and service dispatch).

## Why this matters
- Extends the same cooperative architecture to inter-processor and high-speed link interrupts.
- Keeps interrupt handlers deterministic while shifting protocol/state handling to scheduler tasks.

## Required repository/context inputs for the AI
Include with this prompt:
1. Current module/file layout conventions.
2. Build system and include-path conventions.
3. Existing scheduler event APIs and ISR-safe event set flow.
4. Existing buffering/queue primitives (ring buffer or descriptor/event queues).
5. Existing HAL abstraction interfaces for FSI/IPC (if present).

## Non-negotiable constraints
- Skeletons must compile/link in current repository configuration.
- No direct target-specific register access in generic skeleton files.
- ISRs remain minimal and bounded.
- Service loops must process bounded work per run.
- Event bits are notification hints; multiplicity/payload must be represented in queues/buffers/shared records.

## Design requirements
1. **Minimal ISR contracts for FSI and IPC**
   - Acknowledge interrupt source.
   - Capture minimal metadata (channel/index/status/message slot).
   - Push metadata to queue or update shared record.
   - Set scheduler event bit(s) and exit.
2. **Bounded service-task processing**
   - FSI service drains at most N frames/records per activation.
   - IPC service drains at most N messages/notifications per activation.
   - Fast return on no-work path.
3. **Clear handoff primitives**
   - Distinguish advisory event bits from payload containers.
   - Explicit overflow/backpressure behavior for queues.
4. **HAL boundary clarity**
   - Drivers own HAL/hardware interaction.
   - Services own protocol progression, retries, and state transitions.
5. **Error-path scaffolding**
   - Separate normal event flow from fault/error interrupt flow.
   - Include extension points for timeout/recovery counters and fault escalation.

## Expected AI deliverables
- Skeleton source/header files for FSI and IPC driver/service domains.
- Documentation comments for ISR responsibilities, service responsibilities, and extension points.
- Example wiring into scheduler task registration and event subscription.
- Build verification output.

## Suggested prompt template
```md
Implement Task 7 from `docs/codex_instructions/tasks/07_fsi_ipc_interrupt_service_skeletons.md`.

Create compile-ready, platform-agnostic driver/service skeletons for FSI and IPC interrupt-driven workflows using ISR-to-task handoff.

Return:
1) patch,
2) architecture notes (ISR vs service responsibilities),
3) build/test results.
```

## Acceptance criteria (definition of done)
- FSI/IPC skeleton files compile and link in repo build setup.
- ISR responsibilities are minimal and deterministic.
- Service responsibilities show bounded periodic/event-driven processing.
- Handoff model (event bits + queues/shared records) is explicit and extensible.
- Error/fault flow has clear placeholders for production hardening.
