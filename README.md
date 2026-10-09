# TrackSessionTimer GPS

A new-generation standalone track session timer and GPS lap timer for track-day and club motorsport use.

This repository is a clean hardware and firmware rebuild of the ideas proven in `ctrmint/TrackSessionTimer`. The new design keeps the original focus on an extremely readable session countdown, but adds a larger sunlight-capable display, high-rate GNSS lap timing, session logging and a software architecture intended to grow beyond the limits of the original RP2040/MicroPython platform.

**This is not a CAN logging project.** CAN bus integration is not part of the baseline scope.

## Project status

**Hardware bring-up in progress.**

Running on the Waveshare board: the RM690B0 AMOLED panel over QSPI, FT6336 touch, the
microSD card, the QMI8658 IMU with gyro-tracked attitude, and a gesture-driven UI whose
track catalog is read from the card at boot. A session runs end to end - countdown,
overrun and rest - records its duration and peak G on every axis, writes that to the card
and survives a power cycle.

Not yet implemented: **GNSS**, which is the critical path. The UBX parser exists and is
host tested, but nothing feeds it: there is no transport, so the timing engine, lap state
machine and logger have still never seen a real fix. The receiver and antenna are selected
and in hand; see
[ADR-005](docs/decisions/ADR-005-gnss-transport.md) for the staged I2C-then-UART plan.
There is a GPS Only mode built to read the receiver out once there is one; today it
reports "NO RECEIVER", which is the truth rather than a fault. The RTC is not driven yet.

The repository additionally holds the project plan, architecture, hardware bill of
materials, GNSS timing design, UI requirements, test plan and issue backlog.

## Target hardware

The baseline prototype is built around:

- **Waveshare ESP32-S3-Touch-AMOLED-2.41-B**
  - 2.41 inch AMOLED
  - 600 x 450 pixels
  - 800 cd/m2 stated brightness
  - ESP32-S3R8, 8 MB PSRAM, 16 MB flash
  - capacitive touch
  - onboard QMI8658 IMU
  - RTC and microSD/TF support
  - protective case supplied with the `-B` variant
- **u-blox NEO-M9N GNSS**, initially via the SparkFun GPS-15712 U.FL breakout
  - up to 25 Hz position update rate
  - UART and UBX protocol
  - external antenna support
  - optional timepulse/PPS input to the ESP32 for timing diagnostics
- **External active GNSS antenna** with a clear view of the sky
- **microSD card** for high-rate session logs

The product target is a single dashboard-mounted unit with the display at the front and a compact GNSS/power extension behind it. The development build may use the Waveshare case plus an add-on rear pod before a final enclosure is designed.

See [docs/HARDWARE_BOM.md](docs/HARDWARE_BOM.md) and [hardware/README.md](hardware/README.md).

## Primary goals

1. Preserve the original TrackSessionTimer session countdown workflow.
2. Improve at-a-glance readability in a moving vehicle.
3. Add genuine 20 Hz minimum GNSS, with 25 Hz preferred on the selected M9N hardware.
4. Detect start/finish crossings using a directional virtual line, not a simple radius/geofence trigger.
5. Calculate lap crossing time from GNSS measurement time and interpolate the line intersection between fixes.
6. Record current, previous and best lap times.
7. Add session trace logging to microSD.
8. Retain IMU-based G measurement and launch-related functionality where useful.
9. Operate fully offline at the circuit.
10. Degrade safely if GNSS, SD or IMU functionality becomes unavailable.

## Explicit non-goals for the first release

- CAN bus acquisition or ECU logging
- cloud dependency
- official race timing or transponder replacement
- RTK centimetre-level positioning
- telemetry streaming to a pit wall
- phone dependency during a session
- complex map navigation on the device

These may be evaluated later, but they must not delay the core track timer.

## Firmware direction

The planned firmware stack is **C++ on ESP-IDF**, with LVGL for the user interface once the Waveshare display driver is proven.

The rationale is documented in [docs/decisions/ADR-001-firmware-platform.md](docs/decisions/ADR-001-firmware-platform.md). The original MicroPython project is valuable as a behavioural reference, but the new system has a larger framebuffer, a continuous 20/25 Hz GNSS stream, SD logging and more demanding timing/UI workloads.

The repository starts with a minimal ESP-IDF application under `firmware/` so the toolchain can be validated before display-specific dependencies are added.

## High-level architecture

```text
                  External active GNSS antenna
                              |
                              v
                       +-------------+
                       | u-blox M9N  |
                       | 20/25 Hz    |
                       +------+------+ 
                              | UART / UBX
                              | optional PPS
                              v
+----------------------------------------------------------------+
|                   ESP32-S3 TrackSessionTimer                    |
|                                                                |
|  +-------------+   +---------------+   +-------------------+   |
|  | GNSS ingest |-->| Timing engine |-->| Session state     |   |
|  +-------------+   +---------------+   +-------------------+   |
|          |                 |                     |              |
|          v                 v                     v              |
|  +-------------+   +---------------+   +-------------------+   |
|  | Track match |   | Lap / delta   |   | UI / LVGL         |   |
|  +-------------+   +---------------+   +-------------------+   |
|          |                 |                     |              |
|          +-----------------+----------+----------+              |
|                                       |                         |
|                                       v                         |
|                               +---------------+                 |
|                               | microSD log   |                 |
|                               +---------------+                 |
|                                                                |
|   QMI8658 IMU -> G meter / launch / orientation                |
+----------------------------------------------------------------+
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Lap timing principle

The receiver produces a sequence of timestamped positions. A track file defines directed
start, finish, pit-entry, and pit-exit lines. Start or finish is selected as the repeating
lap boundary; the two pit gates remain independent typed events.

For every new GNSS fix, the timing engine examines the segment between the previous and
current position against all four finite lines. A valid crossing fraction is calculated
geometrically and applied to the GNSS timestamps on either side of the line.

Example:

```text
fix A time        12:43:17.320
fix B time        12:43:17.360
crossing fraction 0.62

crossing time = 17.320 + (0.62 x 40 ms)
              = 17.3448
```

This avoids quantising every lap to the next 40 ms receiver update. It does not remove GNSS position error, so the design still uses quality checks, direction checks, hysteresis, minimum-lap-time rules and trace validation.

See [docs/GNSS_AND_LAP_TIMING.md](docs/GNSS_AND_LAP_TIMING.md).

## Repository layout

```text
.github/                  GitHub issue and PR templates
firmware/                 ESP-IDF firmware project
simulator/                600 x 450 LVGL/SDL desktop device simulator
hardware/                 BOM, wiring and enclosure notes
data/tracks/              Track file schema and synthetic example
data/track-packs/         Provenance-gated offline pack source manifests
planning/                 Export of the live milestone/issue backlog
tests/                    Host-side algorithm tests
tools/                    Development and replay utilities
docs/                     Architecture and project documentation
PROJECT_PLAN.md            Milestones, sequencing and acceptance gates
ROADMAP.md                 Product roadmap beyond the first usable build
CONTRIBUTING.md            Development workflow and quality rules
```

## First development steps

1. Buy or assemble the baseline hardware in `docs/HARDWARE_BOM.md`.
2. Install a supported ESP-IDF toolchain.
3. Build and flash the minimal firmware skeleton.
4. Prove display initialization and full-screen refresh performance.
5. Prove touch, microSD and QMI8658 access.
6. Connect the NEO-M9N on a dedicated UART.
7. Configure the M9N for 20 or 25 Hz UBX output and record raw traces.
8. Develop the lap-crossing engine against recorded traces on a host computer before relying on it on track.

Detailed sequencing is in [PROJECT_PLAN.md](PROJECT_PLAN.md).

## Quick firmware bootstrap

The supported firmware baseline is ESP-IDF v6.0.2. From an activated v6.0.2 shell:

```bash
cd firmware
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Without a native IDF installation, run `make firmware-container-build` from the
repository root. See [docs/DEVELOPMENT_SETUP.md](docs/DEVELOPMENT_SETUP.md) for the
pinned Python environment, clean-clone checks, and flashing notes.

Build the deterministic UK offline track archive from the repository root with
`make uk-track-pack`. The resulting file is written under `build/track-pack/`; it is
ignored by Git and all public-map geometry remains timer-only until validated.

The bootstrap application only proves that the ESP32-S3 toolchain and board connection work. Display, touch and GNSS support are later milestones.

## Hardware-independent screen simulator

**Frozen. No longer developed.** The simulator lags the device and cannot exercise the
gesture-driven interaction model the firmware uses. It is kept in CI as a regression
guard over the shared firmware components it compiles. See
[CONTRIBUTING.md](CONTRIBUTING.md#the-simulator-is-frozen).

The 600 x 450 LVGL/SDL simulator runs the device presentation model without the
Waveshare board. On a Linux host with CMake, Ninja, and SDL2 development headers:

```bash
make simulator-test
make simulator-run
```

The interactive window starts on the ready dashboard. Its large Start, Setup, Review,
and Diagnostics controls accept pointer/touch or keyboard activation and use the same
tested navigation model. Setup provides track selection and transactional editors for
every release-one device option, including explicit Save, Cancel, and confirmed
Defaults actions. The track screen explains all matching/readiness states, requires
confirmation of suggestions, and preserves timer-only operation. Simulator settings
are stored below the operating system temporary directory rather than in the
repository. Deterministic active, GNSS-loss/recovery, and
storage-failure/recovery states remain available through command-line scenarios.
Deterministic backends provide 20/25 Hz synthetic or fixture-driven GNSS, 100 Hz IMU,
RTC, touch, bounded queues, and missing/full/slow/write-failed storage. A containerized
headless check is available with `make simulator-container-test`. See
[simulator/README.md](simulator/README.md) for replay commands, fixture validation,
diagnostics, and the boundary between simulated and physical acceptance.

## Development backlog

The [live GitHub issues](https://github.com/ctrmint/Track-Session-Timer-GPS-/issues)
and milestones are the source of truth. A reviewable epic summary is in
[planning/INITIAL_ISSUES.md](planning/INITIAL_ISSUES.md), with machine-readable
snapshots in [planning/issues.csv](planning/issues.csv) and
[planning/labels.csv](planning/labels.csv).

`tools/create_issues.py` and `tools/create_labels.py` preview the corresponding
GitHub CLI commands. Execute mode skips existing issue titles and forces label updates.

```bash
make issue-preview
make label-preview
```

Use `--repo OWNER/REPO` when seeding another repository. Native sub-issue and
blocked-by relationships are not reconstructed by the CSV preview scripts.

## Design rules

- Timing uses GNSS measurement time, not message-arrival time.
- The active session timer must never depend on Wi-Fi or a phone.
- A GNSS failure must not stop the basic session countdown.
- SD failure must not stop timing.
- UI work must not block GNSS ingestion.
- GNSS raw/derived data must be replayable on a host computer.
- Driver-facing screens prioritise readability over information density.
- No hidden automatic behaviour may alter a completed lap time after it is shown.
- On-track interaction must be minimal.

## Licence

GPL-3.0. The previous TrackSessionTimer repository is GPL-3.0 and this project is expected to reuse or port parts of that work. See [docs/LICENSING.md](docs/LICENSING.md).

## Source references

Hardware claims and baseline component choices were checked against manufacturer/vendor documentation on 2026-08-12. See [docs/SOURCE_REFERENCES.md](docs/SOURCE_REFERENCES.md).
