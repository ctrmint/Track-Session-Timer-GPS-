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

On Fedora:

```bash
sudo dnf install cmake gcc-c++ ninja-build SDL2-devel
```

Build and launch the ready dashboard:

```bash
make simulator-build
make simulator-run
```

Use the large on-screen Start, Setup, Review, and Diagnostics controls with a pointer
or touchscreen. Keyboard focus and Enter activate the same actions. Setup opens a
menu for Track Selection, Device Settings, and G-meter / IMU; every nested screen has
an explicit return path. Start enters the active timer and configuration is then locked. Device
Settings includes every versioned option and explicit Save, Cancel, and confirmed
Defaults controls. Track Selection explains selected, suggested, ambiguous, missing,
invalid, and unavailable states and always offers a timer-only path. Saved simulator
settings, including the selected track, live at
`/tmp/track-session-timer-simulator/settings-v2.bin` on Linux, outside the checkout.
The active timer shows bounded completed-lap feedback while keeping session time
visible. Stopping requires a 1.5-second hold, release, and separate confirmation;
short holds and cancelled presses continue timing. Review shows bounded session history and lap pages, with best/previous emphasis and
explicit REST/READY actions. Diagnostics pages through system, GNSS, logging, and
peripheral health from immutable backend snapshots. All inputs dispatch the same
deterministic navigation actions.

The ready scenario deliberately keeps GNSS in acquisition. It demonstrates that the
session timer remains available while lap timing is unavailable. Use `--scenario`
for deterministic active, GNSS-loss, storage-failure, faster-lap, slower-lap, and
first-lap/no-prior-best test states.

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
  --scenario ready \
  --screen tracks \
  --track-state ambiguous \
  --frames 25 \
  --frame-ms 40 \
  --snapshot build/simulator/tracks.ppm
```

Every headless frame advances scenario time by the requested fixed interval; it does
not use wall-clock timing.
`--screen ready|setup|settings|tracks|g-meter|review|diagnostics` selects the initial
screen. `--track-state selected|missing|invalid|ambiguous|suggested|none|unavailable`
selects a deterministic track fixture and deliberately replaces the persisted track
selection for that run. Omit `--track-state` during normal interactive use so a track
chosen on screen remains selected after restarting the simulator.

Review data is deterministic and never touches the checkout or an SD card:

```bash
build/simulator/track_timer_simulator \
  --screen review \
  --review-state complete
```

`--review-state complete|partial|empty|missing|corrupt|unsupported` exercises history
paging, partial-log warnings, and each safe failure presentation.

Use `--diagnostics-state normal|degraded|missing|recovery` with
`--screen diagnostics` to render stationary subsystem health and retained fault
counters without physical hardware.

Open the hardware-independent G-meter and select an IMU condition with:

```bash
build/simulator/track_timer_simulator --screen g-meter --imu-state normal
build/simulator/track_timer_simulator --screen g-meter --imu-state calibration
build/simulator/track_timer_simulator --screen g-meter --imu-state failure
build/simulator/track_timer_simulator --screen g-meter --imu-state partial
build/simulator/track_timer_simulator --screen g-meter --imu-state recovery
```

`--imu-state normal|calibration|failure|partial|recovery` drives the current marker,
fixed 24-sample trail, session peak marker, directional peak summaries, and explicit
health presentation. The effective display orientation rotates sensor axes and updates
the physical-axis labels. Peak reset is stationary-only. The failure fixture also
verifies that active lap and session time continue without IMU data.

Use `--display-state day|night|dimmed|rotated` for deterministic display policy. These
fixtures override only the in-memory policy input for that run and do not change saved
settings:

```bash
build/simulator/track_timer_simulator --headless --display-state day --frames 4 \
  --snapshot build/simulator/display-day.ppm
build/simulator/track_timer_simulator --headless --display-state night --frames 4 \
  --snapshot build/simulator/display-night.ppm
build/simulator/track_timer_simulator --headless --display-state dimmed \
  --frames 7 --frame-ms 10000 --snapshot build/simulator/display-dimmed.ppm
build/simulator/track_timer_simulator --headless --display-state rotated --frames 4 \
  --snapshot build/simulator/display-rotated.ppm
```

The final status line reports effective brightness, orientation, dim state, layout
shift, and whether a settings draft is being previewed. The Diagnostics peripheral page
shows the same effective policy.

Capture each lap-feedback state without hardware:

```bash
build/simulator/track_timer_simulator --headless --scenario lap-faster --frames 8 \
  --snapshot build/simulator/lap-faster.ppm
build/simulator/track_timer_simulator --headless --scenario lap-slower --frames 8 \
  --snapshot build/simulator/lap-slower.ppm
build/simulator/track_timer_simulator --headless --scenario lap-unavailable-best --frames 8 \
  --snapshot build/simulator/lap-unavailable-best.ppm
```

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
storage recoveries. It also reports logger depth/high-water, producer drops,
unavailable-storage attempts, failed batch attempts, and p95 enqueue-to-write latency.

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
| `lap-faster` | completed lap is 0.750 s faster than the previous best |
| `lap-slower` | completed lap is 1.750 s slower than the previous best |
| `lap-unavailable-best` | first completed lap establishes a best without a false delta |

The session countdown continues through every failure. GNSS quality and logging
availability change on the device screen, while the backend diagnostics retain
emission, recovery, high-water, drop, latency, and write-failure counters. Queues use
the same fixed capacities declared by the firmware domain contracts.

## Simulation boundary

The simulator can validate layout, presenter formatting, state progression, touch
flows, deterministic peripheral replay, fault indication, and recovery. It does
not validate the RM690B0 bus, FT6336 controller, physical GPIOs, PSRAM bandwidth,
microSD latency, GNSS RF/PPS timing, physical luminance, power, or thermal behaviour.

Logical storage latency is modeled without sleeping, so it validates queue pressure
and failure handling rather than a particular microSD card's performance. ESP32-S3
QEMU integration remains tracked by the native sub-issues of GitHub epic #63.
