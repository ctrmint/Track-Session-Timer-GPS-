# ADR-001: Use ESP-IDF and C++ for the new firmware

**Status:** Accepted for prototype

## Context

The previous timer uses MicroPython on RP2040 and a 240 x 240 display. The rebuild adds a 600 x 450 display, continuous 20/25 Hz GNSS parsing, SD logging and a more complex UI.

## Decision

Use ESP-IDF with C++ application/domain code. Introduce LVGL after direct display bring-up.

## Consequences

Positive:

- better control of tasks, queues and memory
- easier performance measurement
- mature ESP32-S3 peripheral support
- LVGL integration path

Negative:

- more complex build environment than MicroPython
- existing application behaviour must be ported rather than copied unchanged
