# Release readiness checklist

This checklist is the release gate for firmware/library integrations using this scheduler and the bundled service skeleton modules.

Use it before tagging a release or shipping production firmware.

## 1) Scheduler correctness checks

Confirm deterministic scheduler behavior with automated tests and target-level smoke checks:

- [ ] Build passes for all supported toolchains/targets.
- [ ] Unit tests pass for scheduler ordering, periodic cadence behavior, and background-task fallback.
- [ ] Task registration boundaries are validated (`SCH_MAX_TASKS`, duplicate/add-failure handling, invalid inputs).
- [ ] Critical-section assumptions are validated on-target (`sch_port_enter_critical`/`sch_port_exit_critical` nesting and restore behavior).
- [ ] Tick source is verified monotonic and stable (`sch_port_now_ticks`) under expected clock and interrupt load.
- [ ] `sch_port_idle()` mapping is validated so idle behavior does not starve periodic work.

Recommended evidence:

- CI test logs for `ceedling test:all`.
- Short hardware run logs showing expected periodic execution rates.

## 2) ISR/task handoff checks

Validate interrupt-to-service transfer correctness under nominal and burst conditions:

- [ ] ISR paths are bounded and minimal (ack/capture/queue/hint only; no blocking or long loops).
- [ ] Producer/consumer ownership is respected (ISR producer, task consumer) for each ring/event queue.
- [ ] Service task budgets drain enough work per scheduler cycle to prevent unbounded lag.
- [ ] Event bits are treated as wakeup hints only; multiplicity/payload are preserved in queues.
- [ ] Synthetic burst tests verify handoff integrity without corruption or missed records beyond explicitly documented drop policy.

Recommended evidence:

- Unit/integration tests for ring/event queue APIs.
- Instrumented burst test output demonstrating bounded backlog recovery.

## 3) Overflow/drop policy validation

Every queue/ring used in ISR-to-task paths must have an explicit overload policy and observability:

- [ ] Overflow behavior is specified per path (drop newest, drop oldest, coalesce, backpressure, or fault escalation).
- [ ] Drop counters/telemetry are exposed and reviewed in test logs.
- [ ] Acceptance thresholds are defined (for example, "0 drops in nominal profile", "bounded drops in stress profile").
- [ ] Overload behavior does not violate safety or control-loop stability requirements.
- [ ] Recovery behavior after overload is verified (system returns to steady-state processing).

Recommended evidence:

- Stress-test logs with drop/overflow counters.
- Requirement trace linking overload policy to system-level safety/performance expectations.

## 4) Porting contract verification

Verify port layer compliance for every target platform:

- [ ] `sch_port_now_ticks()` is monotonic and has documented tick period/resolution.
- [ ] Critical-section hooks correctly preserve/restore interrupt state.
- [ ] `sch_port_idle()` semantics are documented (WFI/no-op/custom low-power gate).
- [ ] Wraparound behavior of tick arithmetic is covered by tests or analysis for expected uptime.
- [ ] Port implementation files are code-reviewed and validated on real hardware.

Recommended evidence:

- Per-target porting note with timer source and critical-section mapping.
- On-target sanity test results.

## skeleton module limitations

The service modules under `include/scheduler/services/` and `src/services/` are **skeletons**, not drop-in production middleware.

To avoid production overclaim, treat the following as explicit limitations unless your product repository has implemented and verified them:

- Protocol completeness is not guaranteed (framing/state-machine/error-retry coverage is intentionally minimal).
- Hardware-driver integration is intentionally thin and may not cover all silicon errata or edge cases.
- Security/safety hardening (input validation, anti-flood controls, fault containment) is incomplete by default.
- Real-time guarantees are workload-dependent and require target-specific WCET/latency characterization.
- Diagnostics/telemetry depth is starter-level and may be insufficient for production observability.
- Certification artifacts (requirements traceability, safety case evidence, formal timing budgets) are not provided out of the box.

## Exit criteria: skeleton -> production-grade services

A service may be considered production-grade only when all criteria below are met:

1. **Requirements complete**
   - Functional, timing, error-handling, and safety/security requirements are baselined and reviewed.
2. **Implementation hardened**
   - Service logic covers required protocol states, retries/timeouts, and fault handling.
   - ISR and task paths remain bounded under verified worst-case input rates.
3. **Verification complete**
   - Unit + integration + target stress tests pass.
   - Negative/fault-injection tests demonstrate safe behavior and recovery.
4. **Observability complete**
   - Production telemetry includes queue depth, drops, error classes, and recovery events.
   - Alert thresholds and field diagnostics are defined.
5. **Portability proven**
   - Porting contract checks pass on every supported target/board configuration.
6. **Operational readiness complete**
   - Documentation includes tuning guidance (budgets, periods, queue sizing).
   - Release notes clearly state validated operating envelope and known residual risks.

All six criteria must be satisfied and signed off in your product process before claiming production readiness.
