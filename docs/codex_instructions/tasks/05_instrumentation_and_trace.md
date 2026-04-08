# Task 5 — Optional Instrumentation and Trace Hooks

## Goal
Add optional per-task timing and miss/overrun visibility with compile-time feature gating.

## Why this matters
- Cooperative schedulers still require observability to detect overload/jitter.
- Enables objective validation in embedded integration.

## Design discussion points
1. Macro-gated instrumentation (`SCH_ENABLE_STATS`, etc.).
2. Data model for runtime stats.
3. Trace callback APIs and execution points.
4. Cost containment in production builds.

## Acceptance notes
- Feature can be fully compiled out.
- No behavior regression when disabled.
