# Track Data

`schema.json` defines the version 3 track-file structure. `synthetic_test_loop.json` is deliberately fictional and exists only for tests/examples.

Firmware accepts schema version 3 definitions up to 16384 bytes. Track identifiers must
fit the persisted 47-character identifier field and use letters, digits, `.`, `_`, or
`-`. Every layout must provide directed `start`, `finish`, `pit_entry`, and `pit_exit`
gates plus revision and provenance metadata. Sector records use the same directed-gate
structure. Each endpoint pair must form a sensible line inside the track geofence after
local projection; start and finish may intentionally contain identical geometry. Run
`make track-validate` for schema and host semantic checks and
`make track-definition-test` for the bounded firmware loader contract. Every file also
declares `provenance.geometry_status`; provisional files are never timing-ready.

Schema versions 1 and 2 are intentionally unsupported: build-time data must be migrated
by supplying all four gates, gate safety parameters, and explicit validation status. Do not commit
private location traces here. Real public circuit definitions require the provenance
and controlled review process in
[`docs/TRACK_CONTRIBUTIONS.md`](../../docs/TRACK_CONTRIBUTIONS.md). Run
`make uk-track-pack` to build the provenance-gated UK deployment archive from
`data/track-packs/uk/manifest.json`.
