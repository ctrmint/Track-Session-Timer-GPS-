# Hardware-independent device simulator

The desktop simulator renders the driver-facing UI at the selected Waveshare panel's
native 600 x 450 logical resolution. It uses the same immutable `UiSnapshot` presenter
as the firmware and replaces only the physical display and touch backend with LVGL's
SDL driver.

LVGL is pinned to **v9.5.0** and verified with the SHA-256 hash in
`simulator/CMakeLists.txt`.

## Native Linux build

Install a C++ compiler, CMake, Ninja, and SDL2 development headers. On Ubuntu 24.04:

```bash
sudo apt-get update
sudo apt-get install --yes cmake g++ libsdl2-dev ninja-build
```

Build and launch the active-session scenario:

```bash
make simulator-build
make simulator-run
```

Click anywhere on the window to cycle through `ready`, `active`, `gnss-loss`, and
`storage-failure` scenarios.

Run deterministic headless smoke tests:

```bash
make simulator-test
```

The tests write PPM captures beneath `build/simulator/`, which is ignored by Git.

## Container validation

If the native SDL/CMake dependencies are unavailable, use:

```bash
make simulator-container-test
```

The container target is intentionally headless. Interactive display forwarding varies
by Linux, macOS, Windows, Wayland, and X11 host configuration, so native execution is
the supported visual workflow.

## Direct options

```bash
build/simulator/track_timer_simulator \
  --headless \
  --scenario gnss-loss \
  --frames 25 \
  --frame-ms 40 \
  --snapshot build/simulator/gnss-loss.ppm
```

Every headless frame advances scenario time by the requested fixed interval; it does
not use wall-clock timing. This makes state and screen generation reproducible.

## Simulation boundary

The simulator can validate layout, presenter formatting, state progression, touch
flows, fault indication, and eventually replay-driven application behaviour. It does
not validate the RM690B0 bus, FT6336 controller, physical GPIOs, PSRAM bandwidth,
microSD latency, GNSS RF/PPS timing, brightness, power, or thermal behaviour.

ESP32-S3 QEMU integration and richer peripheral fixtures remain tracked by the native
sub-issues of GitHub epic #63.
