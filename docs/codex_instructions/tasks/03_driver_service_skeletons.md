# Task 3 — Driver/Service Skeleton Architecture

## Goal
Add platform-agnostic driver/service skeletons showing ISR-to-task bridging patterns for UART, Ethernet, I2C/EEPROM, and motor control supervision.

## Why this matters
- Enforces architectural boundaries: HAL in drivers, business/state logic in services.
- Keeps ISRs short and deterministic.

## Design discussion points
1. Minimal ISR contracts per peripheral.
2. Bounded batch processing in services.
3. Clear handoff primitives (event bits + buffers/queues).
4. HAL abstraction boundary and include dependencies.

## Acceptance notes
- Skeletons compile and link without target-specific register access.
- Files intentionally lightweight and extensible.
