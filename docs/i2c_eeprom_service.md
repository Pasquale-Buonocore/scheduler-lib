# I2C EEPROM Service (`sch_i2c_eeprom_service_t`)

The EEPROM service separates request submission and completion handling:

- request ring stores `sch_eeprom_request_t` items.
- completion queue stores `sch_i2c_event_id_t` IDs.

## API

- `sch_i2c_eeprom_service_init(...)`
- `sch_i2c_eeprom_enqueue_request_isr(...)`
- `sch_i2c_eeprom_isr_complete(...)`
- `sch_i2c_eeprom_service_run(...)`

## How to use

1. Implement `sch_i2c_eeprom_hal_t`:
   - `submit_request` (required)
   - optional `ack_irq`, `consume_completion`
2. Allocate request storage (`sch_eeprom_request_t[]`) and completion storage (`uint16_t[]`).
3. Initialize with `sch_i2c_eeprom_service_init(...)`.
4. Enqueue requests from ISR/signal context with `sch_i2c_eeprom_enqueue_request_isr(...)`.
5. Call `sch_i2c_eeprom_isr_complete(...)` on I2C completion IRQ.
6. Schedule `sch_i2c_eeprom_service_run(...)` as a periodic/background task.

## Tuning guidance

- Increase `request_capacity` when producers can burst.
- Increase `completion_capacity` for dense completion IRQ traffic.
- Tune `max_requests_per_run` and `max_completions_per_run` for bounded execution.
