# Firmware

ESP-IDF v6.0.2 bootstrap for the ESP32-S3 target.

## Build

From an activated native ESP-IDF v6.0.2 shell:

```bash
idf.py set-target esp32s3
idf.py build
```

From the repository root without a native IDF installation:

```bash
make firmware-container-build
```

Do not use an unversioned IDF image for release or CI builds.

## Component layout

```text
components/
  domain/       fixed-size cross-task value contracts
  board/        display, touch, IMU, SD, RTC, power and UART abstraction
  gnss/         receiver transport, configuration and validated fixes
  timing/       crossing geometry and lap state
  track/        track files and local geometry
  session/      countdown and session lifecycle
  logger/       bounded asynchronous logging
  ui/           LVGL presentation and input
  diagnostics/  health and performance aggregation
```

See [components/README.md](components/README.md) for the dependency graph, queue
ownership, and initial capacities.

The component anchors intentionally contain no board assumptions. Hardware
implementations begin only after the exact board revision and schematic have been
checked on the delivered device.
