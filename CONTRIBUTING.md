# Contributing

## Development principles

- Keep timing logic deterministic and testable without the target hardware.
- Keep hardware drivers behind narrow interfaces.
- Do not make the session countdown depend on GNSS, SD, Wi-Fi or any optional subsystem.
- Prefer bounded queues and bounded memory use.
- Avoid dynamic allocation in high-frequency timing paths unless justified and measured.
- Log enough diagnostic state to reproduce timing defects.
- Add tests before changing crossing logic.
- Treat driver readability and low interaction as safety-related UX requirements.

## Branching

Use short-lived branches from `main`:

```text
feature/<issue>-short-description
fix/<issue>-short-description
hardware/<issue>-short-description
```

## Pull requests

Every PR should state:

- linked issue
- behaviour changed
- hardware required to test
- host tests added/changed
- manual tests performed
- timing/performance impact if relevant
- screenshots or photographs for UI changes where practical

## Commit guidance

Prefer small commits that leave the project buildable. Separate refactors from behaviour changes where possible.

## Coding style

- C++17 baseline unless ESP-IDF constraints require otherwise
- `snake_case` functions and variables
- `PascalCase` types
- explicit units in names for ambiguous numeric values, such as `speed_mps`, `timestamp_us`, `h_acc_m`
- avoid naked latitude/longitude pairs where a type can express them
- use monotonic time for MCU duration measurement
- use GNSS time for lap-event calculation

## Testing

Install the pinned host dependencies once, then run all required checks before every PR:

```bash
python -m pip install -r requirements-dev.txt
make check
```

This runs the Python tests, track-schema validation, Markdown and repository hygiene
checks, and the host C++ suites.

**New tests belong in `make check`, as host targets.** Anything that does not genuinely
need LVGL should build and run without it, so its coverage does not depend on the
simulator.

Build the firmware with native ESP-IDF v6.0.2 or the pinned container:

```bash
make firmware-container-build
```

Pull requests must pass the host, simulator and firmware CI jobs. If a hardware/manual
check cannot run, state why and identify the issue that will provide the missing evidence.

## The simulator is frozen

The desktop simulator is **no longer developed**. It lags the device and cannot exercise
the gesture-driven interaction model the firmware now uses, so time spent on it does not
buy confidence in the product.

It stays in CI as a regression guard: it compiles a large part of the firmware's shared
components, so a break there still surfaces. Do not add features or screens to it, and do
not let it constrain device UI decisions.

Note that `simulator/src/` also holds fixtures and file-backed stores that the **host**
suite depends on, such as `track_fixtures.cpp` and `file_track_definition_store.cpp`.
Those are host-test support that happens to live under `simulator/`, and they remain
maintained.

Device behaviour is confirmed on hardware. Every UI defect found during the gesture
rework - an off-centre icon, overlapping text, and corrupt partial redraws on the
RM690B0 - was found on the panel while the simulator suite passed.
