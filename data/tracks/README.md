# Track Data

`schema.json` defines the initial track-file structure. `synthetic_test_loop.json` is deliberately fictional and exists only for tests/examples.

Firmware accepts schema version 1 definitions up to 4096 bytes. Track identifiers must
fit the persisted 47-character identifier field and use letters, digits, `.`, `_`, or
`-`. Start/finish endpoints must form a line of at least one metre after local
projection. Run `make track-validate` for schema checks and
`make track-definition-test` for the bounded firmware loader contract.

Do not commit private location traces here. Real public circuit definitions can be added later with a provenance field and validation process.
