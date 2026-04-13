# IPC Service (`sch_ipc_service_t`)

The IPC service captures compact IRQ records in ISR context and processes them later in task context.

## API

- `sch_ipc_service_init(...)`
- ISR hooks:
  - `sch_ipc_isr_notify(...)`
  - `sch_ipc_isr_ack(...)`
  - `sch_ipc_isr_fault(...)`
- `sch_ipc_service_run(...)`
- `sch_ipc_service_drop_count(...)`

## Record model

ISR builds `sch_ipc_irq_record_t` entries:

- endpoint
- slot
- irq tag
- detail flags

Service run drains records and dispatches to HAL callbacks:

- `service_notify`
- `service_ack`
- `service_fault`

## How to use

1. Fill a `sch_ipc_hal_t` callback table.
2. Allocate static `sch_ipc_irq_record_t record_storage[]`.
3. Initialize with `sch_ipc_service_init(...)` and chosen overflow mode.
4. Connect hardware IRQ handlers to the ISR API.
5. Register `sch_ipc_service_run(...)` as a scheduler task.
6. Monitor `sch_ipc_service_drop_count(...)` for overload telemetry.

## Tuning guidance

- Choose `SCH_OVERFLOW_DROP_NEWEST` for preserving older record order.
- Choose `SCH_OVERFLOW_DROP_OLDEST` to prioritize newest state.
- Set `max_records_per_run` high enough to recover from bursts.
