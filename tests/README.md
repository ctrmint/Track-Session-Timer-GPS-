# Tests

Run:

```bash
python -m unittest discover -s tests -p 'test_*.py'
```

The suite currently covers:

- dependency-free reference geometry
- Draft 2020-12 track schema validation
- fixed-size C++ domain contracts and queue-capacity assumptions
- deterministic simulator scenarios and shared UI presentation formatting
- local Markdown/repository hygiene through `make check`

Run `make simulator-test` to build LVGL/SDL and execute headless 600 x 450 screen
smoke tests. Recorded-trace replay and stateful timing tests are added as the timing
engine is implemented.
