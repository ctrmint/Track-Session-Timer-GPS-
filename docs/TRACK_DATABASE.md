# Track Database

## 1. Objective

Store track timing geometry locally so the device works offline.

## 2. File model

One JSON file per track is simplest during development. A later packed database can be added if loading time becomes a problem.

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

## 5. Versioning

Every track file includes `schema_version`. Session logs record the track file hash or revision so later edits cannot make an old timing result impossible to reproduce.
