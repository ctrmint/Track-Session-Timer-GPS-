# Requirements

## Functional requirements

### Session timing

- FR-001: User can configure a track session duration.
- FR-002: Device shows a large countdown during an active session.
- FR-003: Session progress uses an obvious visual progression.
- FR-004: Session timing continues if GNSS is unavailable.
- FR-005: Device supports a controlled overtime state.

### GNSS

- FR-010: Device supports at least 20 position updates per second.
- FR-011: Baseline hardware should support 25 Hz so 20 versus 25 Hz can be evaluated.
- FR-012: GNSS data includes receiver measurement time, latitude, longitude, speed, heading and quality indicators.
- FR-013: GNSS configuration is applied and verified at startup.
- FR-014: Loss of valid fix is visibly indicated.
- FR-015: The device records the effective GNSS update rate and dropped-message counters.

### Lap timing

- FR-020: A track definition contains a start/finish line and valid crossing direction.
- FR-021: A lap event is generated when the vehicle path segment intersects the start/finish line in the valid direction.
- FR-022: Crossing time is interpolated between surrounding GNSS measurement timestamps.
- FR-023: Duplicate crossings are suppressed using geometry, hysteresis and a minimum lap time.
- FR-024: Current, previous and best lap are available to the UI.
- FR-025: Every lap event is reproducible from the stored session trace.
- FR-026: User can enable or disable Trackday Mode as a persisted setting.
- FR-027: Trackday Mode shows countdown and estimated laps during a session while hiding live lap results; GNSS-derived laps remain logged and are available after the session stops.

### Storage

- FR-030: Device logs session metadata to microSD.
- FR-031: Device logs GNSS fixes at the configured high rate.
- FR-032: Storage failure does not stop active timing.
- FR-033: Logs contain firmware version, hardware profile and track definition identifier.

### IMU

- FR-040: Onboard QMI8658 remains available for G display and orientation features.
- FR-041: IMU failure does not stop timer or lap timing.

### UI

- FR-050: Essential values are readable at a glance.
- FR-051: Active driving mode minimises touch interactions.
- FR-052: GPS state is obvious without requiring a menu.
- FR-053: Lap event feedback is visible but does not obscure session time for an unsafe duration.

## Non-functional requirements

- NFR-001: Timing path must not block on screen refresh or SD write.
- NFR-002: High-rate GNSS ingestion uses bounded memory.
- NFR-003: Device boots into a useful degraded session timer if optional subsystems fail.
- NFR-004: Firmware should recover cleanly from a sudden power cycle.
- NFR-005: All crossing logic has host-side deterministic tests.
- NFR-006: Logging should support at least a full track day on a normal microSD card with large capacity margin.
- NFR-007: No cloud service or phone is required at the circuit.
- NFR-008: Firmware update and configuration actions cannot interrupt an active session accidentally.
- NFR-009: The design must expose enough diagnostics to distinguish GNSS quality problems from software timing problems.

## Performance targets

These are engineering targets, not promises of certified timing accuracy.

- GNSS input: 20 Hz minimum, 25 Hz preferred if stable
- UART: 230400 baud initial target, subject to measurement
- GNSS queue: enough headroom for at least 2 seconds of fixes without dropping the oldest unprocessed item silently
- display: driver-facing values should update smoothly without requiring a full 25 Hz screen redraw
- lap event calculation: completed before the next valid GNSS fix under normal operation
- SD logging: zero dropped GNSS records during a 30-minute bench/outdoor stress test
- cold boot to usable timer: target under 5 seconds, GNSS fix acquisition may continue asynchronously
