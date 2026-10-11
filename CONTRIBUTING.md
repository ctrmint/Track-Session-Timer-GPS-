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
- When two code paths answer the same question differently, document both and link the
  issue that will decide between them. Describing one as though it were the behaviour is
  worse than describing neither, because it reads as settled.

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

**Link issues with `Refs #N` unless the PR genuinely completes them.** GitHub's closing
keywords cannot be negated: a body containing the words "does not close #18" still closes
#18 on merge, because the parser sees the keyword and the number and ignores the prose
around them. Keep `Closes`, `Fixes` and `Resolves` away from an issue number entirely
unless closing is what you want. An issue with hardware acceptance criteria that software
cannot satisfy is not complete, however finished the code is.

## Commit guidance

Prefer small commits that leave the project buildable. Separate refactors from behaviour changes where possible.

**Make a claim true before you commit it.** A message that says something is "recorded on
the issue", "documented in" or "tracked by" asserts the existence of something outside the
diff, and nobody reviewing the diff can see whether it is there. Post the comment, write
the section or raise the issue first, then commit.

**Do not cite an external URL you have not just opened.** `make check` validates local
Markdown links only, so a plausible-looking external link - a vendor product page, a
datasheet, a standard - ships silently and is discovered by whoever needs it. Reuse a URL
already in the repository, or verify the one you are adding.

## Coding style

- C++17 baseline unless ESP-IDF constraints require otherwise
- `snake_case` functions and variables
- `PascalCase` types
- explicit units in names for ambiguous numeric values, such as `speed_mps`, `timestamp_us`, `h_acc_m`
- say what a distance is measured *between*, not only its unit. A gate's 12 m is a total
  width; a 13.3 m separation between two roads is centreline to centreline. Comparing the
  two directly is a units-correct answer that is wrong by half a road, and that mistake has
  been made in this repository
- avoid naked latitude/longitude pairs where a type can express them
- use monotonic time for MCU duration measurement
- use GNSS time for lap-event calculation

## Testing

Create the pinned environment once, then run all required checks before every PR:

```bash
make venv
make check
```

`make venv` builds `.venv` from `requirements-dev.txt`, and the Makefile uses it whenever it
exists, so it does not need activating. Do not install the requirements into whatever
`python` happens to resolve to: the versions are pinned because one of them changes what the
validators *accept* rather than whether they run. `make check` refuses an interpreter that
cannot validate what the suite asserts, and `docs/DEVELOPMENT_SETUP.md` explains why.

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
