# Tests

Run:

```bash
python -m unittest discover -s tests -p 'test_*.py'
```

The suite currently covers:

- dependency-free reference geometry
- Draft 2020-12 track schema validation
- fixed-size C++ domain contracts and queue-capacity assumptions
- deterministic 20/25 Hz GNSS replay and versioned fixture validation
- simulated touch, IMU, RTC, storage faults, bounded queues, and recovery
- simulator scenarios and shared UI presentation formatting
- local Markdown/repository hygiene through `make check`

Run `make simulator-test` to build LVGL/SDL and execute headless 600 x 450 failure,
recovery, and recorded-fixture smoke tests. Run `make simulator-fixture-validate` to
check fixture format and data ranges directly.
