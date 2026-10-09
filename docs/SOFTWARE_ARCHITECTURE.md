# Software Architecture

## 1. Platform

Target:

- ESP32-S3
- ESP-IDF
- C++17 application code
- FreeRTOS tasking provided by ESP-IDF
- LVGL for UI after board support is established

## 2. Layering

```text
Application
  session_controller
  lap_controller
  ui_presenter

Domain
  timing
  track
  session models

Services
  gnss
  logger
  settings
  diagnostics

Hardware abstraction
  display
  touch
  imu
  sd
  rtc
  power
  uart
```

The domain layer should be compilable/testable on a host with minimal ESP-specific dependencies.

## 3. Component contracts

### `GnssFix`

Immutable observation from one receiver epoch.

### `LapEvent`

Contains:

- lap index
- crossing GNSS timestamp
- lap duration
- line interpolation fraction
- quality snapshot
- optional flags such as `suspect_accuracy`

### `UiSnapshot`

A presentation-only copy containing already-formatted or simple values. The UI must not call into the GNSS parser.

### `LogRecord`

Tagged record passed to the logger queue. Log serialization happens in the logger task, not in the timing task.

`AsyncLogger` provides the fixed 256-record multi-context boundary. Producers use a
non-blocking enqueue; the dedicated ESP-IDF `session_logger` task drains batches of up
to 16 through `StorageBackend`. Queue contention/capacity drops, storage failures, and
enqueue-to-write latency percentiles are observable. A staged batch is retained across
storage failure, and state transitions request an explicit buffered flush.

## 4. Concurrency

GNSS parser and timing engine need predictable latency. Avoid direct SD writes, LVGL calls or settings persistence from those paths.

Recommended queue flow:

```text
GNSS transport (I2C for bring-up, UART before the vehicle - ADR-005)
    |
    v
GNSS pipeline task: parse -> judge -> count
    |
    +--> latest GNSS status
    |
    v
fix queue
    |
    v
Timing task ----> lap event queue ----> session controller
    |                                      |
    +---------------> log queue <----------+
                           |
                           v
                   session_logger task ---> buffered storage backend

session controller -------------------------------> UI snapshot
```

A lost fix has no symptom of its own: the device keeps running, the screen keeps updating,
and the only trace is a lap time that does not repeat. So every way one can go missing is
counted separately, because they have different causes and different remedies, and a single
"dropped" total would conflate a slow consumer with a starved bus.

Two of those counters deserve care when they are read. Gaps are measured from the
receiver's own time of week rather than from arrival cadence: arrival says when this system
got round to looking, which on a polled bus is a statement about the CPU rather than about
the receiver. And a gap is only visible once the stream resumes, because a hole cannot be
seen until its far edge arrives - until then loss reads as silence, which is why receiver
health and the gap counter are separate instruments rather than one.

## 5. Memory

The 600 x 450 display is much larger than the original 240 x 240 panel. UI memory strategy must be measured early.

Prefer:

- PSRAM for large display buffers where supported by the driver
- bounded queues
- fixed-capacity recent-fix ring buffers
- no unbounded in-RAM session history
- logs streamed to SD

The UI baseline uses two 600 x 40 RGB565 partial buffers (96,000 bytes total), with
external RAM preferred. See [UI_FOUNDATION.md](UI_FOUNDATION.md) for the buffer
contract, simulator measurements, and hardware-validation boundary.

## 6. Configuration

Use versioned configuration structures. Suggested domains:

- user/session settings
- hardware profile
- GNSS profile
- track database
- UI preferences

Never silently reinterpret an old configuration version. Migrate explicitly or fall back to known defaults.

## 7. Diagnostics

Expose a diagnostic screen containing at least:

- firmware version
- uptime
- reset reason
- free internal RAM / PSRAM
- SD state
- GNSS rate
- GNSS fix type
- satellite count
- horizontal accuracy
- GNSS queue high-water mark
- maximum poll interval, which is what explains a gap on a polled transport
- fixes lost, counted separately by cause: bytes the transport never handed over, frames
  the parser discarded, fixes the quality gate refused, epochs the receiver never sent,
  and fixes dropped because the consumer was too slow
- logger queue high-water mark
- SD write failures
- display frame/update metrics
- IMU state

`DiagnosticsSnapshot` is the immutable service-to-UI boundary. The stationary screen
shows bounded System, GNSS, Logging, and Peripheral pages and never calls a backend
directly. Unavailable hardware and values that a backend does not simulate are
different states. Recovery changes current subsystem health but retains drop, write
failure, and recovery counters so an intermittent fault is not silently erased.

Display policy is similarly hardware-independent. Settings, stationary/activity state,
day/night selection, and an optional sensed orientation produce a bounded
`board::DisplayCommand`. Firmware policy never calls a panel driver directly; later
hardware integration implements the `DisplayOutput` interface. The simulator renders
the same brightness, orientation, dim, and AMOLED-shift commands and exposes them in
diagnostics.

IMU presentation uses the same boundary. `ImuMeterInput` carries one timestamped board
sample plus per-axis validity, calibration state, and the effective display
orientation. `ImuMeterController` rotates samples into driver-relative longitudinal
and lateral axes, retains a fixed 24-point ring, and calculates bounded session peaks.
The LVGL G-meter consumes only its snapshot. A missing or partial sensor therefore
changes presentation health but cannot call, block, or stop the session timer. The
future QMI8658 adapter remains behind `board::ImuInput`.

## 8. Watchdog

Use watchdogs to detect dead tasks, but do not disguise recurrent software faults with endless silent resets. Store a bounded reset reason/diagnostic record where possible.

## 9. Dependencies

Keep third-party dependencies explicit and pinned once hardware bring-up succeeds. Do not add a large library for a small geometry function without a clear reason.

LVGL integration should use a supported ESP-IDF path rather than mixing unrelated Arduino assumptions into the main firmware.
