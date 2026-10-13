# Development Setup

## Supported baseline

The project is pinned to:

- ESP-IDF **v6.0.2**
- ESP32-S3 target
- C++17 application code
- Python **3.13** for repository tools and CI
- LVGL **v9.5.0** for the desktop device simulator
- `espressif/idf:v6.0.2` for reproducible container builds

ESP-IDF v6.0 supports Python 3.10 through 3.14, but use Python 3.13 for
project host tools unless a change is reviewed in a pull request. Do not build release
artefacts with an unversioned `latest` IDF image or an arbitrary IDF branch.

## Host prerequisites

Required for all development:

- Git
- Python 3.13
- a C++17 compiler
- GNU Make
- Docker, or a native ESP-IDF v6.0.2 installation

The normal host checks do not require the target board.

```bash
make venv
make check
```

`make venv` creates `.venv` and installs the pinned `requirements-dev.txt`. The Makefile
uses `.venv/bin/python` whenever that file exists, so the environment does not need to be
activated for `make` targets; activate it only to run tools directly.

Do not run the checks against a system Python that happens to have the packages. The
versions are pinned because one of them changes what the validators *accept*, not just
whether they run: `jsonschema` enforces a `format` keyword only when a checker for that
format is installed, and says nothing when one is not. Without `rfc3339-validator`,
`format: date-time` is not checked, so `provenance.verified_utc` will accept any string -
including in `tools/validate_tracks.py` and `tools/track_workbench.py`, which is the field
the track provenance gate reads. `make check` runs `tools/check_python_env.py` first to
refuse that interpreter rather than let it quietly widen what the validators allow.

On Windows, activate the virtual environment with the appropriate PowerShell or
Command Prompt script before running the same Python and Make targets.

## Desktop device simulator

The hardware-independent simulator requires CMake, Ninja, and SDL2 development
headers in addition to the normal host prerequisites. On Ubuntu 24.04:

```bash
sudo apt-get update
sudo apt-get install --yes cmake g++ libsdl2-dev ninja-build
make simulator-test
make simulator-run
```

The simulator opens a fixed 600 x 450 ready dashboard. Use its large controls with a
pointer/touchscreen, or keyboard focus and Enter. Every input path dispatches the same
deterministic navigation actions. Setup edits the complete versioned device-settings
record and makes Save, Cancel, and Defaults confirmation explicit. It also provides a
local track selector with deterministic selected, suggested, ambiguous, missing,
invalid, and unavailable fixtures. On Linux its
persistent simulator record is kept at
`/tmp/track-session-timer-simulator/settings-v2.bin`, never in the checkout. Active,
GNSS-loss/recovery, and
storage-failure/recovery scenarios are selectable from the command line. The same
presenter remains free of SDL and ESP-IDF dependencies so it can be compiled into both
host and firmware builds.

Use the built-in synthetic GNSS loop at 25 Hz, or replay the checked-in versioned
fixture at 20 Hz:

```bash
build/simulator/track_timer_simulator --headless --scenario active --gnss-rate 25
build/simulator/track_timer_simulator \
  --headless --scenario active --gnss-rate 20 \
  --gnss-fixture simulator/fixtures/recorded_reference_v1.csv
make simulator-fixture-validate
```

The host backends also simulate 100 Hz IMU samples, RTC progression, bounded touch
input, storage latency, and missing/full/write-failed storage. Their counters make
queue high-water marks, drops, write failures, and recovery observable without real
hardware or wall-clock delays.

For a headless container validation without installing native build dependencies:

```bash
make simulator-container-test
```

See [the simulator guide](../simulator/README.md) for direct scenario and snapshot
commands. Physical display, touch-controller, GPIO, RF, power, and performance
acceptance still require the board.

## Reproducible container build

The least ambiguous firmware build uses Espressif's versioned IDF image:

```bash
make firmware-container-build
```

Equivalent command:

```bash
docker run --rm \
  -u "$(id -u):$(id -g)" \
  -e HOME=/tmp \
  -e IDF_GIT_SAFE_DIR=/work \
  -v "$(pwd):/work" \
  -w /work/firmware \
  espressif/idf:v6.0.2 \
  bash -lc "idf.py set-target esp32s3 && idf.py build"
```

The build output is generated under `firmware/build/` and is ignored by Git.

## Native ESP-IDF installation

Use Espressif's Installation Manager or the release tag. A direct Linux/macOS
installation from the exact tag is:

```bash
git clone --branch v6.0.2 --recursive \
  https://github.com/espressif/esp-idf.git esp-idf-v6.0.2
cd esp-idf-v6.0.2
./install.sh esp32s3
. ./export.sh
idf.py --version
```

The reported version must be `v6.0.2`. Re-run the release's `export.sh` in every new
shell before using `idf.py`.

Then build this repository:

```bash
cd firmware
idf.py set-target esp32s3
idf.py build
```

## Flash and monitor after the board arrives

Connect the board with a data-capable USB cable and identify its serial device.

```bash
cd firmware
idf.py -p /dev/ttyACM0 flash monitor
```

Replace `/dev/ttyACM0` with the actual port. On Linux, confirm the current user has
access through the distribution's serial-device group and udev rules. Do not use
permanent root execution as the normal fix for a permissions problem.

Expected bootstrap messages include:

```text
TrackSessionTimer GPS bootstrap
sd-card: mounted
settings: source=0 current-format=1, mode track-day
track catalog: built (26 discovered, 26 loaded, 0 rejected, cap 32)
imu: ready
gnss: transport attached over i2c
ready screen presented on the 600x450 panel
```

The track counts depend on what is on the card. The settings source reports where the
stored configuration came from, and a non-zero value means it was migrated from an older
format on this boot.

### Reading the GNSS stage line

`gnss: transport attached over i2c` means only that the I2C driver accepted an address:
adding a device to the bus never talks to the chip, so that line says nothing about whether
a receiver is present. What follows it does. The firmware logs the receiver's stage whenever
it changes, and the satellite count once a second while searching:

```text
gnss: configuration acknowledged (20 Hz, nav-pvt on, nmea off, raw write form, frames 1, bytes 10)
gnss: bus ok, receiver silent (sats 0, polls 1, bytes 0, frames 0, ...)
gnss: fix accepted (sats 15, polls 31, bytes 4900, frames 49, ..., accepted 49, rejected 0, 19.9 Hz)
```

The configuration line comes first and matters most: a NEO-M9N in its factory default emits
NMEA, which this firmware does not parse, so without a successful configuration every line
after it reports a healthy receiver nobody can read. `acknowledged` means the receiver
answered `UBX-ACK-ACK`; anything else is logged as a warning, because every later symptom -
no satellites, no fix, no lap - traces back to it.

Once fixed, the measured cadence is logged whenever it changes. That figure is derived from
the receiver's own measurement timestamps, so it reports what arrived rather than what was
asked for, and `rejected` climbing alongside `accepted` means fixes are being refused by the
quality gate - the next line names which rule.

The stages are ordered the way the link actually fails, and each has a different cause:

| Stage | What it means |
|---|---|
| `bus unreadable` | the bus itself cannot be read: wiring, address, or pull-ups |
| `bus ok, receiver silent` | the bus works and the receiver declares nothing waiting |
| `bytes arriving, no valid frame` | bytes arrive but nothing passes checksum - usually NMEA |
| `ubx arriving, no nav-pvt` | valid UBX, but not the message asked for: configuration (#16) |
| `searching` | NAV-PVT arriving and refused by the quality gate: a normal cold start |
| `fix accepted` | usable for timing |

`bytes arriving, no valid frame` is the one stage the counters cannot diagnose on their own,
because neither NMEA nor a corrupted bus produces a checksum error - a checksum is only
reached after a sync pair, and neither has one. So that stage also logs the first bytes
received, as text:

```text
gnss: first traffic [64 bytes] "$GNRMC,..."
```

A readable NMEA sentence means the receiver is healthy and in its factory default, which is
a receiver to configure rather than wiring to re-check. Unreadable bytes mean the opposite.
Note that an NMEA sentence contains a position, so a captured log is as sensitive as the
place it was captured.

If flashing waits for download, use the board's documented BOOT/RESET sequence. Do
not freeze board GPIO assignments until the exact 2.41-B schematic and physical board
revision have been checked.

### Capturing a cold boot log

`idf.py monitor` is interactive and refuses to run without a TTY attached, so it cannot be
scripted. For a scriptable capture, including everything printed before a host would
normally attach:

```bash
make firmware-capture-boot
# or, to keep it:
python3 -B tools/capture_boot_log.py --seconds 15 --output boot.log
```

This holds the port open, resets the chip itself, and reads straight through the boot. Two
traps are worth knowing, because both fail in ways that look like something else:

- **Opening the port and waiting captures nothing.** Boot output is written and discarded
  before a host attaches, so the listener has to be holding the port before the reset.
- **Pulsing RTS alone boots the download stub, which prints nothing.** RTS is wired to EN
  (reset) and **DTR to GPIO0** (boot select), and pyserial asserts both when it opens a
  port. Releasing RTS with GPIO0 still low starts the ROM download stub, and an empty
  capture then reads as "the output was discarded" when it means "the application never
  ran". DTR must be deasserted before the open and again after it.

A genuine **power cycle cannot be captured** this way at all. The USB device is inside the
chip, so cutting power destroys the port the listener is holding. A reset through RTS is a
chip reset — `rst:0x15 (USB_UART_CHIP_RESET)` in the log — and the handle survives it. Only
a separate USB-UART adapter on UART0 could see a post-power-cycle boot, since its own
connection to the host is independent of the target's power.

**UART0 is not used as a diagnostic tap, by decision.** The Waveshare's UART socket is
GPIO43/44, which is UART0, and
[ADR-005](decisions/ADR-005-gnss-transport.md) reserves those pins for the GNSS receiver:
UART is pushed by DMA where I2C must be polled, so the vehicle configuration needs them. A
console and a receiver cannot share the socket — log output would be transmitted into the
receiver, and the console driver would own the pin the receiver answers on.

So a post-power-cycle boot stays unobservable, and that is an accepted limit rather than an
open problem. It is also a narrow one: a chip reset reproduces everything a power cycle
does except the power-on reset itself, and that is captured above.

`pyserial` is deliberately not in `requirements-dev.txt`, being useful only with hardware
attached. Install it into the environment when needed:
`.venv/bin/python -m pip install pyserial`.

## Clean-clone verification

A second environment should be able to run:

```bash
git clone git@github.com:ctrmint/Track-Session-Timer-GPS-.git
cd Track-Session-Timer-GPS-
make venv
make check
make firmware-container-build
git status --short
```

The final command should report no generated or modified tracked files.

## Troubleshooting

- `idf.py: command not found`: activate the v6.0.2 IDF environment or use the container target.
- `cannot run the host checks`: the interpreter is missing a pinned dependency, or has
  one that `jsonschema` has not registered as a format checker. Run `make venv`. A
  distribution's own `python3-jsonschema` package commonly omits
  `rfc3339-validator`, which produces a single confusing failure in
  `tests/test_track_schema.py` while every other target still passes.
- Docker permission denied: configure the current user for the local Docker service; do not run repository builds as root unless the resulting ownership is understood.
- SDL2 or CMake missing: install the simulator prerequisites above or run `make simulator-container-test`.
- Serial port unavailable: verify the cable, device path, group membership, and that no monitor process already owns the port.
- Target mismatch: run `idf.py set-target esp32s3` and rebuild.
- A vendor example fails: record its exact IDF/LVGL versions; do not silently change the project-wide IDF pin.

## References

- ESP-IDF v6.0.2 ESP32-S3 guide: https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32s3/
- ESP-IDF release: https://github.com/espressif/esp-idf/releases/tag/v6.0.2
- LVGL v9.5.0 release: https://github.com/lvgl/lvgl/releases/tag/v9.5.0
- IDF Docker image guide: https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32s3/api-guides/tools/idf-docker-image.html
- Waveshare board guide: https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.41
