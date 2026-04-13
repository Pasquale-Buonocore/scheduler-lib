# Ethernet Service (`sch_eth_service_t`)

The Ethernet service uses an event queue (`uint16_t` IDs) to hand off ISR events to task context.

## API

- `sch_eth_service_init(...)` initializes HAL hooks and event queue.
- ISR hooks enqueue events:
  - `sch_eth_isr_rx(...)`
  - `sch_eth_isr_tx(...)`
  - `sch_eth_isr_link(...)`
- `sch_eth_service_run(...)` drains events and calls HAL processing hooks.

## Event model

Queued events:

- `SCH_ETH_EVENT_RX_READY`
- `SCH_ETH_EVENT_TX_DONE`
- `SCH_ETH_EVENT_LINK_CHANGE`

`sch_eth_service_run(...)` maps those IDs to:

- `fetch_rx_descriptor(...)`
- `complete_tx_descriptor(...)`
- `service_link_state(...)`

## How to use

1. Provide a `sch_eth_hal_t` with at least `ack_irq`.
2. Allocate static `uint16_t event_storage[]`.
3. Call `sch_eth_service_init(...)`.
4. Route MAC/PHY interrupts to the corresponding ISR hooks.
5. Register `sch_eth_service_run` in the scheduler.

## Tuning guidance

- Increase `event_capacity` for IRQ bursts.
- Tune `max_events_per_run` to balance Ethernet responsiveness with fairness to other tasks.
