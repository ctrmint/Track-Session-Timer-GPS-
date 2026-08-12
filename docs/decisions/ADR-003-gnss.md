# ADR-003: Prototype with u-blox NEO-M9N at 20/25 Hz

**Status:** Accepted for prototype

## Decision

Use NEO-M9N, initially through SparkFun GPS-15712, over a dedicated UART. Develop at 20 Hz first and evaluate 25 Hz once the full system is stable.

## Reasons

- exceeds the mandatory 20 Hz requirement
- established UBX binary protocol
- external antenna support
- multi-constellation standard-precision receiver
- development breakout is readily documented

## Consequences

- standard-precision GNSS position noise remains a fundamental timing error source
- antenna installation is critical
- receiver power and high-rate serial traffic must be measured
