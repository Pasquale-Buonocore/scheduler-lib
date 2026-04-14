# UART Service (`sch_uart_service_t`)

The UART service defers interrupt work into scheduler task context using two SPSC rings:

- RX ring (`uint16_t` bytes) for received data.
- TX ring (`uint16_t` bytes) for bytes queued to transmit.

## API

- `sch_uart_service_init(...)` initializes HAL hooks, buffers, and run budgets.
- `sch_uart_isr_rx(...)` captures one received byte in ISR context.
- `sch_uart_isr_tx_ready(...)` signals that TX hardware can accept data.
- `sch_uart_service_queue_tx(...)` queues bytes for deferred transmit.
- `sch_uart_service_run(...)` drains TX and RX rings in bounded loops.

## How to use

1. Provide a `sch_uart_hal_t` implementation:
   - `try_read_byte`, `try_write_byte`
   - optional `ack_irq`
2. Allocate static `uint16_t` storage arrays for RX/TX rings.
3. Call `sch_uart_service_init(...)` once at startup.
4. Wire ISR hooks:
   - RX IRQ -> `sch_uart_isr_rx(...)`
   - TX-empty/TX-ready IRQ -> `sch_uart_isr_tx_ready(...)`
5. Register `sch_uart_service_run` as a scheduler task.
6. Queue outbound bytes using `sch_uart_service_queue_tx(...)`.

## Tuning guidance

- Increase `rx_capacity` for bursts of incoming data.
- Increase `tx_capacity` for larger outbound bursts.
- Set `max_rx_items_per_run`/`max_tx_items_per_run` to bound runtime while avoiding starvation.
