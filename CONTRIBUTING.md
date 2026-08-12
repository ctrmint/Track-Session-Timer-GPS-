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

Run host tests before every PR:

```bash
python -m unittest discover -s tests -p 'test_*.py'
```

When firmware dependencies are installed:

```bash
cd firmware
idf.py build
```
