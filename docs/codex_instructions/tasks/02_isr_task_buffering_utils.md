# Task 2 — ISR→Task Buffering Utilities

## Goal
Create reusable utility modules for transferring data captured in ISRs to scheduler tasks.

## Why this matters
- Event bits should indicate "work exists", not carry payload.
- Stream/burst data (UART RX, descriptor indices) requires queues/buffers.

## Design discussion points
1. SPSC ring buffer API and ownership model.
2. Overflow behavior:
   - drop newest
   - drop oldest
   - count drops
3. Power-of-two optimization vs generic sizing.
4. Optional event/index queue module for descriptor IDs.

## Acceptance notes
- ISR-producer/task-consumer safety documented.
- Deterministic bounded operations.
- Unit tests cover ordering and overflow.
