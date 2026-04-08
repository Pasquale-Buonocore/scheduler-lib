# Task 5 — Optional Instrumentation and Trace Hooks

## Purpose of this task file
Use this file as a **prompt-ready spec** for adding optional scheduler instrumentation with strict compile-time gating.

## Implementation goal
Add optional per-task timing and miss/overrun visibility with compile-time feature flags.

## Why this matters
- Cooperative schedulers still require observability to detect overload/jitter.
- Enables objective validation during embedded integration.

## Required repository/context inputs for the AI
When prompting with this file, include:
1. Current scheduler task execution flow.
2. Existing macro/config patterns.
3. Any current stats/diagnostics structures.
4. Build variants (debug/release or feature toggles).

## Non-negotiable constraints
- Instrumentation must be fully compilable out.
- Disabled mode must not alter runtime behavior.
- Runtime overhead in enabled mode should be explicit and bounded.
- Public APIs must remain stable unless change is clearly justified.

## Design requirements
1. **Macro-gated instrumentation**
   - e.g., `SCH_ENABLE_STATS` and related gates.
2. **Stats data model**
   - per-task counters/timestamps/miss or overrun fields.
   - clear reset/read patterns.
3. **Trace callbacks/hooks**
   - define execution points (task start/end, miss, etc.).
   - document callback contract and reentrancy assumptions.
4. **Cost containment**
   - avoid unnecessary branches/allocations when disabled.
   - keep added code straightforward for embedded review.

## Expected AI deliverables
- Instrumentation and/or trace hook implementation.
- Compile-time configuration wiring.
- Documentation for enabled/disabled behavior.
- Tests or checks demonstrating no-regression when disabled.

## Suggested prompt template
```md
Implement Task 5 from `docs/codex_instructions/tasks/05_instrumentation_and_trace.md`.

Add compile-time-gated scheduler instrumentation and trace hooks with clear performance boundaries.

Return:
1) patch,
2) config/usage notes,
3) verification that disabled mode has no behavior regression.
```

## Acceptance criteria (definition of done)
- Feature can be fully compiled out.
- No behavior regression when instrumentation is disabled.
- Enabled mode exposes per-task timing/miss/overrun observability.
- Cost/overhead considerations are documented.
