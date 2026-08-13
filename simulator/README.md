# Hardware-independent device simulator

The desktop simulator renders the driver-facing UI at the selected Waveshare panel's
native 600 x 450 logical resolution. It uses the same immutable `UiSnapshot` presenter
as the firmware and deterministic host backends for the physical device interfaces.

LVGL is pinned to **v9.5.0** and verified with the SHA-256 hash in
`simulator/CMakeLists.txt`.

## Native Linux build

Install a C++ compiler, CMake, Ninja, and SDL2 development headers. On Ubuntu 24.04:

```bash
sudo apt-get update
sudo apt-get install --yes cmake g++ libsdl2-dev ninja-build
```

Build and launch the ready dashboard:

```bash
make simulator-build
make simulator-run
```

Use the large on-screen Start, Setup, Review, and Diagnostics controls with a pointer
or touchscreen. Keyboard focus and Enter activate the same actions. Setup, Review,
and Diagnostics return through their Back control; Start enters the active timer and
configuration is then locked. All inputs dispatch the same deterministic navigation
actions.

The ready scenario deliberately keeps GNSS in acquisition. It demonstrates that the
session timer remains available while lap timing is unavailable. Use `--scenario`
for deterministic active, GNSS-loss, and storage-failure test states.

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

## Peripheral replay

The simulator provides deterministic implementations of the shared clock, GNSS,
touch, IMU, RTC, and storage interfaces. GNSS supports the production target rates:

```bash
build/simulator/track_timer_simulator \
  --headless \
  --scenario active \
  --gnss-rate 20 \
  --gnss-fixture simulator/fixtures/recorded_reference_v1.csv \
  --frames 50
```

Omit `--gnss-fixture` to use the built-in synthetic loop. Both 20 Hz and 25 Hz
advance from simulated monotonic time, never wall time. The final status line includes
the fixture and rate, active fault modes, queue-drop count, storage failures, and
storage recoveries.

Checked-in CSV fixtures use an explicit version marker and fixed schema. Validate
them with:

```bash
make simulator-fixture-validate
```

See [fixtures/README.md](fixtures/README.md) for the version 1 format and provenance
rules.

## Deterministic fault schedules

| Scenario | Simulated behaviour |
|---|---|
| `ready` | GNSS remains in acquisition while the session is idle |
| `active` | 20/25 Hz GNSS, 100 Hz IMU, RTC progression, and ready storage |
| `gnss-loss` | loss for 2 s, stale fixes for 1 s, corrupt fixes for 1 s, then recovery |
| `storage-failure` | missing, full, slow, and write-failed storage for 1 s each, then recovery |

The session countdown continues through every failure. GNSS quality and logging
availability change on the device screen, while the backend diagnostics retain
emission, recovery, high-water, drop, latency, and write-failure counters. Queues use
the same fixed capacities declared by the firmware domain contracts.

## Simulation boundary

The simulator can validate layout, presenter formatting, state progression, touch
flows, deterministic peripheral replay, fault indication, and recovery. It does
not validate the RM690B0 bus, FT6336 controller, physical GPIOs, PSRAM bandwidth,
microSD latency, GNSS RF/PPS timing, brightness, power, or thermal behaviour.

Logical storage latency is modeled without sleeping, so it validates queue pressure
and failure handling rather than a particular microSD card's performance. ESP32-S3
QEMU integration remains tracked by the native sub-issues of GitHub epic #63.
