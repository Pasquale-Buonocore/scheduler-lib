# FSI Service (`sch_fsi_service_t`)

The FSI service defers RX/TX/error interrupt handling by storing compact IRQ records.

## API

- `sch_fsi_service_init(...)`
- ISR hooks:
  - `sch_fsi_isr_rx(...)`
  - `sch_fsi_isr_tx(...)`
  - `sch_fsi_isr_error(...)`
- `sch_fsi_service_run(...)`
- `sch_fsi_service_drop_count(...)`

## Record model

ISR captures `sch_fsi_irq_record_t`:

- channel
- frame index
- IRQ tag
- status

Task-side run dispatches records to HAL callbacks:

- `service_rx_frame`
- `service_tx_complete`
- `service_error`

## How to use

1. Implement `sch_fsi_hal_t` for target-specific driver integration.
2. Allocate static `sch_fsi_irq_record_t record_storage[]`.
3. Call `sch_fsi_service_init(...)` with ring capacity, budget, and overflow mode.
4. Call ISR hook functions from each FSI interrupt source.
5. Register `sch_fsi_service_run(...)` as scheduler work.
6. Track `sch_fsi_service_drop_count(...)` to detect saturation.

## Tuning guidance

- Increase `record_capacity` for interrupt storms.
- Tune `max_records_per_run` to bound runtime while keeping up with IRQ production.
- Select overflow mode according to history-vs-recency priority.
