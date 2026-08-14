# Track Database

## 1. Objective

Store track timing geometry locally so the device works offline.

## 2. File model

The database uses one JSON file per circuit layout. The firmware parser is allocation-free and
accepts at most 16384 bytes, 16 sector records, a 47-byte identifier, a 63-byte display
name, and a three-character country value plus its terminator. A later packed database
can be added if measured loading time requires it.

Schema version 2 fields:

```json
{
  "schema_version": 2,
  "revision": 1,
  "track_id": "example_circuit",
  "name": "Example Circuit",
  "country": "GB",
  "provenance": {
    "source": "survey or authoritative public source",
    "license": "source licence identifier",
    "verified_utc": "2026-08-14T00:00:00Z"
  },
  "reference": {
    "lat_deg": 52.000000,
    "lon_deg": -1.000000
  },
  "geofence": {
    "center_lat_deg": 52.000000,
    "center_lon_deg": -1.000000,
    "radius_m": 3000
  },
  "gates": {
    "start": {
      "left": {"lat_deg": 52.000010, "lon_deg": -1.000020},
      "right": {"lat_deg": 51.999990, "lon_deg": -0.999980},
      "direction_heading_deg": 90.0,
      "heading_tolerance_deg": 60.0,
      "minimum_crossing_speed_mps": 2.0,
      "rearm_corridor_m": 15.0
    },
    "finish": {
      "left": {"lat_deg": 52.000010, "lon_deg": -1.000020},
      "right": {"lat_deg": 51.999990, "lon_deg": -0.999980},
      "direction_heading_deg": 90.0,
      "heading_tolerance_deg": 60.0,
      "minimum_crossing_speed_mps": 2.0,
      "rearm_corridor_m": 15.0
    },
    "pit_entry": {
      "left": {"lat_deg": 52.000110, "lon_deg": -1.000020},
      "right": {"lat_deg": 52.000090, "lon_deg": -0.999980},
      "direction_heading_deg": 90.0,
      "heading_tolerance_deg": 60.0,
      "minimum_crossing_speed_mps": 1.0,
      "rearm_corridor_m": 10.0
    },
    "pit_exit": {
      "left": {"lat_deg": 51.999910, "lon_deg": -1.000020},
      "right": {"lat_deg": 51.999890, "lon_deg": -0.999980},
      "direction_heading_deg": 90.0,
      "heading_tolerance_deg": 60.0,
      "minimum_crossing_speed_mps": 1.0,
      "rearm_corridor_m": 10.0
    }
  },
  "timing": {
    "minimum_lap_time_s": 30
  },
  "sectors": [
    {
      "sector_id": "sector_1",
      "name": "Sector 1",
      "gate": {
        "left": {"lat_deg": 52.000010, "lon_deg": -0.999520},
        "right": {"lat_deg": 51.999990, "lon_deg": -0.999480},
        "direction_heading_deg": 90.0,
        "heading_tolerance_deg": 60.0,
        "minimum_crossing_speed_mps": 2.0,
        "rearm_corridor_m": 15.0
      }
    }
  ]
}
```

The repository includes a synthetic example and a JSON Schema under `data/tracks/`.
Left and right are viewed in the configured travel direction. Gates are finite line
segments: a crossing detector may use the corridor only to rearm after a crossing, not
as a timing box. Start and finish may be identical when a layout has one shared line.

## 3. Track matching

Automatic suggestion should use only a broad geofence. It must never create a lap event from the geofence.

Flow:

1. valid GNSS fix acquired
2. find tracks whose broad geofence contains the position
3. if exactly one plausible match, expose it as a suggestion requiring confirmation
4. if ambiguous, require selection
5. the selected lap gate still uses exact line geometry

`match_track_geofences` is a fixed-capacity decision service over a caller-owned catalog
of at most 16 validated definitions. It uses great-circle distance and reports these
states explicitly: location unavailable, no match, one suggestion, ambiguity, persisted
manual selection, selected definition missing, or invalid catalog. A selected track is
usable offline and is never replaced automatically merely because another geofence is
nearby. Duplicate identifiers invalidate the catalog instead of silently choosing one.

The result includes the nearest catalog entry for explanation, but only definitions
whose radius contains the position become suggestion candidates. The matching API has
no lap-event operation; timing remains exclusively owned by exact selected-gate crossing
logic.

The Setup > Track Selection screen consumes this result through an allocation-free
view model. It lists validated local definitions, shows whether exact gate
geometry is ready, and requires the driver to confirm a suggested track. A confirmed
track identifier is persisted through `SettingsManager`; the UI has no direct storage
path. Selecting Timer Only clears the identifier. Both actions are rejected after a
session becomes active, including a race between opening the screen and pressing the
action. Deterministic simulator fixtures cover selected, missing, invalid, ambiguous,
suggested, no-nearby-track, and unavailable-location states.

## 4. Unknown track capture

`track_timer/track/capture.hpp` provides the shared allocation-free capture primitive.
It accepts a timing-quality GPS fix only while speed is at most 0.5 m/s and no session
is active. Position accuracy must be 5 m or better. The user supplies the expected
travel heading because GNSS course-over-ground is not trustworthy while stationary;
the primitive derives left/right endpoints perpendicular to that heading and returns
a preview without modifying an earlier preview on failure.

`tools/track_workbench.py` provides the corresponding host workflow. It can create a
complete definition from four centre/heading/width captures, refine one gate while
advancing the revision and provenance, validate a candidate, and atomically import or
export a database file. Every import runs both JSON Schema and semantic geometry checks.
For example:

```bash
python3 -B tools/track_workbench.py validate data/tracks/synthetic_test_loop.json
python3 -B tools/track_workbench.py refine candidate.json finish \
  --center-lat 52.0 --center-lon -1.0 --heading 90 --width-m 20 \
  --verified-utc 2026-08-14T10:00:00Z --output candidate-r2.json
python3 -B tools/track_workbench.py import candidate-r2.json \
  --database-dir /path/to/device/tracks
```

Run `python3 -B tools/track_workbench.py new --help` for the four repeated `--gate`
arguments used to create a definition without rebuilding firmware. Device menu wiring,
four-gate progress, and persistent on-device storage remain in issue #108. No capture
flow may require driver interaction at speed.

## 5. Loading, projection, and versioning

Every track file includes `schema_version`; the current loader accepts exactly version 2.
Unsupported versions, malformed JSON, missing fields, out-of-range geometry, capacity
overflow, unknown members, duplicate sector identifiers, gates outside the geofence,
and implausible gate length/direction are rejected. Reports include a stable field path,
and a failed load never modifies the caller's current active definition.

On a successful load, all eight gate endpoints are projected to local east/north metres
relative to the definition's reference point. The loaded object also carries a
fixed-width 64-bit FNV-1a fingerprint over the exact file bytes. Session metadata can
record the schema version, track identifier, and 16-character fingerprint so the exact
source definition can be recovered; whitespace-only file edits deliberately produce a
new fingerprint.

Version 1 files are not inferred or silently upgraded because they contain only one
line and cannot safely invent pit entry/exit geometry. A build-time migration must add
all four gates, per-gate safety parameters, and the track-level minimum lap time before
the file can be deployed.

The firmware-facing contract is `track_timer/track/definition.hpp`. File-system and
NVS/SD-card adapters remain outside the parser, so malformed storage input cannot gain
ownership of timing state.
