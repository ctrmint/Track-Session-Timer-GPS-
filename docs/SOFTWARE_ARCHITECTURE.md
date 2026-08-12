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

## 4. Concurrency

GNSS parser and timing engine need predictable latency. Avoid direct SD writes, LVGL calls or settings persistence from those paths.

Recommended queue flow:

```text
UART ISR/driver
    |
    v
GNSS parser task
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
                                       UI snapshot
```

## 5. Memory

The 600 x 450 display is much larger than the original 240 x 240 panel. UI memory strategy must be measured early.

Prefer:

- PSRAM for large display buffers where supported by the driver
- bounded queues
- fixed-capacity recent-fix ring buffers
- no unbounded in-RAM session history
- logs streamed to SD

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
- dropped GNSS records
- logger queue high-water mark
- SD write failures
- display frame/update metrics
- IMU state

## 8. Watchdog

Use watchdogs to detect dead tasks, but do not disguise recurrent software faults with endless silent resets. Store a bounded reset reason/diagnostic record where possible.

## 9. Dependencies

Keep third-party dependencies explicit and pinned once hardware bring-up succeeds. Do not add a large library for a small geometry function without a clear reason.

LVGL integration should use a supported ESP-IDF path rather than mixing unrelated Arduino assumptions into the main firmware.
