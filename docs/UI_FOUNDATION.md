# UI foundation

## Dependency and boundary

LVGL is pinned to `9.5.0` in both supported build paths:

- ESP-IDF resolves `lvgl/lvgl` through the component-manager manifest in
  `firmware/components/ui/idf_component.yml`.
- CMake FetchContent resolves the same release and SHA-256 for the SDL simulator.

`domain::UiSnapshot` remains a fixed-size, trivially-copyable value. The presenter
consumes only a const snapshot and creates a `DeviceViewModel`; LVGL objects consume
only that view model. UI code does not call GNSS, logger, or storage services.

## 600 x 450 display-buffer baseline

The hardware-independent baseline is two partial RGB565 buffers, each 600 pixels by
40 lines:

| Item | Value |
|---|---:|
| Bytes per pixel | 2 |
| Bytes per partial buffer | 48,000 |
| Buffer count | 2 |
| Total display-buffer budget | 96,000 bytes |
| Preferred allocation | External RAM / PSRAM |

The strategy is expressed by `ui::kDeviceDisplayBuffers` and guarded by compile-time
tests. The eventual panel adapter must fall back deliberately if external RAM is not
available; it must not silently allocate two full frames in internal RAM. DMA
capability and final line count still require validation on the delivered board.

## Visual system

The shared foundation defines:

- 8, 12, 20, and 30 pixel spacing steps and a minimum 56 pixel touch target;
- caption, body, heading, secondary-timer, and primary-timer typography roles;
- semantic positive, caution, warning, critical, overtime, and logging colours;
- WCAG relative-luminance contrast selection for dynamic foreground text;
- fixed-cell timer fields so changing numerals cannot move time values.

Critical states also carry text or numeric cues. GNSS uses the satellite symbol plus
`NO GPS`, `SEARCH`, `POOR`, `GOOD`, or `STALE`. Session progression uses `SESSION`,
`UNDER 20 MIN`, `UNDER 10 MIN`, `FINAL 5 MIN`, or `OVERTIME`; colour is supplementary.

## Host profile

The simulator reports render and LVGL memory metrics on every run. A 100-frame
headless active-session run on the Fedora development host measured:

| Metric | Measurement |
|---|---:|
| Average update + render | 3,376 microseconds |
| Maximum update + render | 5,573 microseconds |
| Maximum LVGL heap in use | 28,920 bytes |

The simulator uses its own 32-bit SDL direct-render surface. The LVGL heap measurement
does not include SDL-owned surface memory, and host timing is not an ESP32-S3
performance claim. These values establish a reproducible regression baseline until
the actual panel, DMA, and PSRAM path can be profiled.

Run the profile without writing repository files:

```bash
SDL_VIDEODRIVER=dummy build/simulator/track_timer_simulator \
  --headless --scenario active --frames 100
```
