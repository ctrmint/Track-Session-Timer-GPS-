# System Architecture

## 1. Context

The original TrackSessionTimer demonstrated the session workflow on an integrated RP2040 round display. The new repository treats session timing as one capability inside a larger standalone track instrument.

The fundamental architecture rule is separation: GNSS acquisition, lap timing, display rendering and storage must be able to run at different rates without blocking each other.

## 2. Hardware blocks

```text
Vehicle USB / battery
        |
        v
+---------------------+          +----------------------+
| Waveshare           | UART     | NEO-M9N breakout     |
| ESP32-S3 AMOLED     |<-------->| 20/25 Hz GNSS        |
| 2.41-B              |          | external antenna     |
+----------+----------+          +----------------------+
           |
           +-- QMI8658 IMU
           +-- FT6336 touch
           +-- RM690B0 AMOLED
           +-- RTC
           +-- microSD
```

A later custom daughterboard can replace breakout wiring with a compact integrated GNSS and power board while retaining the same software interface.

## 3. Firmware domains

### Board support package

Owns:

- display
- touch
- brightness
- IMU
- SD
- RTC
- board-specific pin mapping

No application timing logic belongs here.

### GNSS service

Owns:

- UART reception
- UBX framing and checksum
- receiver configuration
- fix quality validation
- conversion to internal `GnssFix`
- update-rate and data-loss metrics
- optional PPS diagnostics

### Track service

Owns:

- track-file loading
- schema validation
- nearest-track selection
- start/finish line geometry
- optional sectors

### Timing engine

Owns:

- line crossing test
- crossing direction
- time interpolation
- lap state
- minimum-lap-time rules
- duplicate suppression
- current/previous/best results

It does not draw the UI and does not write directly to SD.

### Session service

Owns:

- ready/configuration/running/rest states
- countdown duration
- overtime state
- post-session summary lifecycle
- integration of lap state and timer state

### Logger

Owns:

- bounded queue from GNSS/timing/session producers
- session metadata
- buffered SD writes
- file rollover/close
- drop/error counters

### UI

Owns presentation and user input. It consumes immutable snapshots of application state rather than reaching directly into GNSS or storage drivers.

## 4. Task model

Suggested FreeRTOS tasks:

| Task | Priority | Typical rate | Responsibility |
|---|---:|---:|---|
| GNSS RX/parser | high | byte stream / 20-25 Hz fixes | Never lose serial data |
| Timing engine | high | 20-25 Hz | Consume validated fixes and generate lap events |
| Logger | medium | buffered | Drain records to SD |
| UI | medium | 10-30 Hz internal, lower expensive redraw rate | Render state and handle touch |
| IMU | medium/low | 50-100 Hz as required | G/orientation data |
| Housekeeping | low | 1-10 Hz | battery, diagnostics, persistence |

Exact priorities are measured, not guessed. The design must avoid priority inversion around SD and display locks.

## 5. Data ownership

Use immutable or copyable value objects between tasks:

- `GnssFix`
- `GnssStatus`
- `LapEvent`
- `LapState`
- `SessionState`
- `ImuSample`
- `UiSnapshot`

Avoid sharing driver objects across tasks unless access is explicitly serialized.

## 6. Timing clocks

Three concepts are deliberately separate:

1. **GNSS measurement time**: authoritative for interpolated lap crossing.
2. **ESP monotonic time**: authoritative for local durations, watchdogs and message age.
3. **RTC/calendar time**: useful for filenames and session metadata, not lap timing.

The GNSS service records both GNSS time and arrival monotonic time so transport latency can be measured.

## 7. Failure behaviour

| Failure | Required behaviour |
|---|---|
| GNSS no fix | Session countdown continues; lap values show unavailable/stale state |
| GNSS UART fails | Attempt controlled reinitialisation; no timer reset |
| SD missing/full/error | Continue timing in RAM; visible logging warning |
| IMU fails | Disable G/orientation dependent feature; timing unaffected |
| touch fails | Active session remains visible; recovery requires restart or external controls later |
| display driver fails | watchdog/restart policy evaluated; logs should preserve diagnostic reason if possible |

## 8. Future expansion boundary

CAN, BLE telemetry, Wi-Fi sync and external sensors should connect through optional service interfaces. They must not change the timing engine's dependency graph.
