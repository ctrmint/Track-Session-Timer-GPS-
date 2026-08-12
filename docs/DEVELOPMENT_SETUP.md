# Development Setup

## 1. Toolchain

The project targets ESP-IDF on ESP32-S3. During planning, current Espressif documentation exposes stable/release branches for ESP32-S3. Pin an exact ESP-IDF version once the Waveshare board support and display driver are proven.

For the first toolchain test, use a current supported ESP-IDF installation and record the exact version in your first hardware issue.

## 2. Build the bootstrap firmware

```bash
cd firmware
idf.py set-target esp32s3
idf.py build
```

Flash:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

Replace the serial path as required.

## 3. Expected bootstrap output

The starter application should print a banner similar to:

```text
TrackSessionTimer GPS bootstrap
Hardware bring-up not yet implemented
```

## 4. Host tests

No third-party Python packages are required for the initial geometry tests.

```bash
python -m unittest discover -s tests -p 'test_*.py'
```

## 5. Waveshare board support

Use the official Waveshare examples/schematic to identify:

- RM690B0 display initialization
- QSPI pin mapping
- FT6336 touch mapping
- QMI8658 I2C mapping
- SD/TF interface
- RTC
- free UART-capable GPIOs

Create the board support layer from known working vendor examples, then isolate it from application logic.

## 6. LVGL

LVGL supports ESP-IDF integration and Espressif provides an `esp_lvgl_port` component path. Do not add LVGL until a minimal direct display test works on the target board. This separates display-driver problems from UI-framework problems.

## 7. GNSS development

Before connecting GNSS to firmware timing code:

1. verify receiver output with a USB/UART adapter or known-good host if useful
2. configure the target update rate
3. save a representative UBX trace
4. commit only small anonymised/synthetic fixtures to the repository, not private full-day location logs

## 8. GitHub workflow

Suggested sequence:

```bash
git checkout -b hardware/001-display-bringup
# work
git add ...
git commit -m "Bring up AMOLED display"
git push -u origin hardware/001-display-bringup
```

Open a pull request and attach measurements/screenshots where relevant.
