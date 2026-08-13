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
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements-dev.txt
make check
```

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
record and makes Save, Cancel, and Defaults confirmation explicit. On Linux its
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
GNSS fix queue capacity: 64
Hardware bring-up not yet implemented
```

If flashing waits for download, use the board's documented BOOT/RESET sequence. Do
not freeze board GPIO assignments until the exact 2.41-B schematic and physical board
revision have been checked.

## Clean-clone verification

A second environment should be able to run:

```bash
git clone git@github.com:ctrmint/Track-Session-Timer-GPS-.git
cd Track-Session-Timer-GPS-
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements-dev.txt
make check
make firmware-container-build
git status --short
```

The final command should report no generated or modified tracked files.

## Troubleshooting

- `idf.py: command not found`: activate the v6.0.2 IDF environment or use the container target.
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
