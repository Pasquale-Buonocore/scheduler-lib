# Motor Supervision Service (`sch_motor_supervision_service_t`)

The motor service combines command application, feedback ingestion, and fault handling.

## API

- `sch_motor_supervision_service_init(...)`
- `sch_motor_supervision_set_command(...)`
- ISR hooks:
  - `sch_motor_isr_feedback(...)`
  - `sch_motor_isr_fault(...)`
- `sch_motor_supervision_service_run(...)`

## Behavior summary

- Latest command (`enable`, `target_speed_rpm`) is applied each run.
- Feedback IRQ path can sample feedback and queue a feedback event.
- Fault IRQ queues a fault event; service run disables motor when fault event is consumed.

## How to use

1. Provide `sch_motor_hal_t` with required command callbacks:
   - `set_enable`
   - `set_target_speed_rpm`
2. Optionally provide `ack_irq` and `read_feedback_sample`.
3. Allocate feedback storage (`sch_motor_feedback_t[]`) and event storage (`uint16_t[]`).
4. Call `sch_motor_supervision_service_init(...)`.
5. Update targets via `sch_motor_supervision_set_command(...)`.
6. Route motor interrupts to ISR hooks.
7. Schedule `sch_motor_supervision_service_run(...)`.

## Tuning guidance

- `feedback_capacity` should cover worst-case sample bursts.
- `event_capacity` should cover combined feedback/fault event bursts.
- Tune per-run budgets for deterministic latency.
