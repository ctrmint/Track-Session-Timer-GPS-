# Track Data

`schema.json` defines the version 2 track-file structure. `synthetic_test_loop.json` is deliberately fictional and exists only for tests/examples.

Firmware accepts schema version 2 definitions up to 16384 bytes. Track identifiers must
fit the persisted 47-character identifier field and use letters, digits, `.`, `_`, or
`-`. Every layout must provide directed `start`, `finish`, `pit_entry`, and `pit_exit`
gates plus revision and provenance metadata. Sector records use the same directed-gate
structure. Each endpoint pair must form a sensible line inside the track geofence after
local projection; start and finish may intentionally contain identical geometry. Run
`make track-validate` for schema and host semantic checks and
`make track-definition-test` for the bounded firmware loader contract.

Schema version 1 is intentionally unsupported: build-time data must be migrated by
supplying the three additional gates and the new gate safety parameters. Do not commit
private location traces here. Real public circuit definitions require the provenance
and validation process tracked by issue #109.
