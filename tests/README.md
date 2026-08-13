# Tests

Run:

```bash
python -m unittest discover -s tests -p 'test_*.py'
```

The suite currently covers:

- dependency-free reference geometry
- Draft 2020-12 track schema validation
- fixed-size C++ domain contracts and queue-capacity assumptions
- local Markdown/repository hygiene through `make check`

Recorded-trace replay and stateful timing tests are added as the timing engine is implemented.
