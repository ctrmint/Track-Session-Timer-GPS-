# Track Database

## 1. Objective

Store track timing geometry locally so the device works offline.

## 2. File model

Release one uses one JSON file per track. The firmware parser is allocation-free and
accepts at most 4096 bytes, 16 sector records, a 47-byte identifier, a 63-byte display
name, and a three-byte country value including its terminator. A later packed database
can be added if measured loading time requires it.

Proposed schema fields:

```json
{
  "schema_version": 1,
  "track_id": "example_circuit",
  "name": "Example Circuit",
  "country": "GB",
  "reference": {
    "lat_deg": 52.000000,
    "lon_deg": -1.000000
  },
  "geofence": {
    "center_lat_deg": 52.000000,
    "center_lon_deg": -1.000000,
    "radius_m": 3000
  },
  "start_finish": {
    "a": {"lat_deg": 52.000010, "lon_deg": -1.000020},
    "b": {"lat_deg": 51.999990, "lon_deg": -0.999980},
    "direction_heading_deg": 90.0,
    "heading_tolerance_deg": 60.0,
    "minimum_lap_time_s": 30
  },
  "sectors": []
}
```

The repository includes a synthetic example and a JSON Schema under `data/tracks/`.

## 3. Track matching

Automatic suggestion should use only a broad geofence. It must never create a lap event from the geofence.

Flow:

1. valid GNSS fix acquired
2. find tracks whose broad geofence contains the position
3. if exactly one plausible match, suggest/select it according to user preference
4. if ambiguous, require selection
5. start/finish crossing still uses exact line geometry

## 4. Unknown track capture

Safe future workflow:

- configure/capture while stationary before entering the circuit where possible
- allow user to record a start-line centre point and expected heading
- derive an initial perpendicular line of configured width
- after the session, allow refinement from logged traces on a host tool

Do not require a driver to interact with configuration screens at speed.

## 5. Loading, projection, and versioning

Every track file includes `schema_version`; release one accepts exactly version 1.
Unsupported versions, malformed JSON, missing fields, out-of-range geometry, capacity
overflow, and a start/finish line shorter than one metre are rejected explicitly. A
failed load never modifies the caller's current active definition.

On a successful load, both start/finish endpoints are projected to local east/north
metres relative to the definition's reference point. The loaded object also carries a
fixed-width 64-bit FNV-1a fingerprint over the exact file bytes. Session metadata can
record the schema version, track identifier, and 16-character fingerprint so the exact
source definition can be recovered; whitespace-only file edits deliberately produce a
new fingerprint.

The firmware-facing contract is `track_timer/track/definition.hpp`. File-system and
NVS/SD-card adapters remain outside the parser, so malformed storage input cannot gain
ownership of timing state.
