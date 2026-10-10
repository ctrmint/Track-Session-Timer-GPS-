# Track Database

## 1. Objective

Store track timing geometry locally so the device works offline.

## 2. File model

The database uses one JSON file per circuit layout. The firmware parser is allocation-free and
accepts at most 16384 bytes, 16 sector records, a 47-byte identifier, a 63-byte display
name, and a three-character country value plus its terminator. A later packed database
can be added if measured loading time requires it.

Schema version 3 fields:

```json
{
  "schema_version": 3,
  "revision": 1,
  "track_id": "example_circuit",
  "name": "Example Circuit",
  "country": "GB",
  "provenance": {
    "source": "survey or authoritative public source",
    "license": "source licence identifier",
    "verified_utc": "2026-08-14T00:00:00Z",
    "geometry_status": "physically_validated"
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

`provenance.geometry_status` is mandatory and machine-readable. `provisional` files may
be browsed and refined but cannot be selected for lap timing. `device_captured`,
`independently_validated`, and `physically_validated` are timing-ready. The timing
configuration adapter independently rejects provisional definitions, so an old saved
identifier cannot bypass the selection-screen guard.

### 2.1 Promoting geometry beyond provisional

The status names imply a process. This is it.

Promotion is the one step that turns unverified data into a lap time a driver will
believe, so it is never a judgement call recorded in a commit message. Each status has
evidence that must exist before it may be claimed:

| Status | What it requires |
| --- | --- |
| `provisional` | Derived from a public map. The default, and what every untouched definition stays at. |
| `independently_validated` | **At least two sources from distinct origins, agreeing within 10 m.** Two readings of the same database are one source read twice. |
| `device_captured` | All four gates captured on the device while stationary, or from a logged trace of the circuit. |
| `physically_validated` | Captured and then confirmed by driving it, with lap times that repeat. |

Ten metres is the threshold because it is 0.16 s at 62.6 m/s, the fastest a UK circuit is
driven. A start/finish line displaced *along* the straight shifts every lap by the same
amount, so lap durations and lap-to-lap comparisons are unaffected; what it changes is
absolute agreement with another timing system. GPS lap timing carries roughly 0.15 to
0.25 s of error of its own, so demanding better agreement would be false precision -
and demanding worse would admit a line on the wrong part of the circuit entirely.

For the UK pack, the evidence lives in the gate profile's `validation` block in
`data/track-packs/uk/manifest.json`, and `tools/build_uk_track_pack.py` enforces every
rule above at build time. A promotion that does not meet them **stops the build** rather
than quietly producing a provisional pack nobody notices. The full citation travels with
the pack in `source-manifest.json`; the definition's own `provenance.source` carries a
compact form because the schema caps that field at 127 characters.

Derivation never promotes. `tools/derive_osm_gates.py` always produces `provisional`, and
a test holds it to that, so geometry cannot become timing-ready as a side effect of being
regenerated.

### 2.2 Why this gate is not theoretical

The first circuit put through this process, Donington Park, was found to be **wrong** -
not merely unvalidated.

Its provisional start/finish gate sat exactly on OpenStreetMap way 841515325, *Melbourne
Loop (up)*. Donington's start/finish line is on the Wheatcroft Straight, way 242867013,
which the pit lane opens onto at both ends. The gate was 82 m from that straight, 131 m
from the corrected position, and its heading was 162 degrees out - pointing back down the
circuit. A car crossing the real line would never have triggered it; the direction check
would have refused it even if it had.

Both Donington layouts share that profile, so neither the GP nor the National circuit
could have timed a lap, and the National circuit does not even drive the Melbourne Loop.

Three further venues carry start/finish gates taken from features that are not their
start/finish straight: Croft from a way named *Rallycross*, Silverstone's international
profile from *Stowe Circuit* (a separate circuit on the same site), and Pembrey from
*Honda*, a corner. Four more are taken from unnamed or whole-circuit ways, where the name
alone cannot say whether the position is right. These remain provisional and are tracked
in #139.

The lesson is worth keeping: the provenance gate was not protecting against imprecision.
It was the only thing standing between unverified geometry and confident, wrong lap times.

## 3. Track matching

Automatic suggestion should use only a broad geofence. It must never create a lap event from the geofence.

Flow:

1. valid GNSS fix acquired
2. find tracks whose broad geofence contains the position
3. if exactly one plausible match, expose it as a suggestion requiring confirmation
4. if ambiguous, require selection
5. the selected lap gate still uses exact line geometry

`match_track_geofences` is a fixed-capacity decision service over a caller-owned catalog
of at most 32 validated definitions. It uses great-circle distance and reports these
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
suggested, no-nearby-track, and unavailable-location states. Provisional definitions
remain visible as capture candidates, are labelled `PROVISIONAL - TIMER ONLY`, and
cannot be persisted as the active timing track.

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
arguments used to create a definition without rebuilding firmware.

On the device, open **Setup > Track Selection**, browse to a validated layout, then
choose **Capture Info**. The capture screen walks through the left and right endpoints
of Start, Finish, Pit Entry, and Pit Exit, showing the current coordinate, horizontal
accuracy, fix age, 8-point completion count, and a line-length/heading preview. The
vehicle must be stationary, the session must be stopped, the fix must be no more than
two seconds old, and horizontal accuracy must be 5 m or better. Save stays disabled
until all eight endpoints are captured and the complete definition passes the same
semantic validation as a prebuilt file.

Each layout is written as its own versioned JSON file through the
`TrackDefinitionStore` interface. The simulator adapter uses a temporary file and a
last-known-good backup, validates before promotion, restores an interrupted write, and
requires explicit confirmation before overwriting an existing layout. Cancel and every
capture/save rejection leave the previous valid file untouched. Host-created JSON can
still be imported, selected, refined on the device, and reloaded without rebuilding
firmware. No capture flow permits driver interaction at speed.

Saving all eight stationary endpoints promotes the saved revision to
`device_captured`. Refining only one gate in the host workbench preserves the prior
validation status unless a reviewer explicitly supplies `--geometry-status`.

## 5. UK offline track pack

`data/track-packs/uk/manifest.json` defines the reviewed major-circuit scope: 35
standard layouts at 18 venues across England, Scotland, Wales, and Northern Ireland.
Official venue pages establish layout identity. Raceway context is derived from
OpenStreetMap under ODbL 1.0 with the required attribution retained in both source and
package manifests.

Run `make uk-track-pack` to create `build/track-pack/uk-track-pack-v1.zip`. At pack
revision 2 the archive contains 24 schema-valid definitions, a hash for every one, and 11
explicit blockers where public four-gate geometry could not be identified safely. No
blocked layout receives invented coordinates.

Two of those definitions are timing-ready - Donington Park GP and National, which share
one validated gate profile. The other 22 carry `timing_ready: false` and must go through
section 2.1 before they can be used for lap timing. The package is deterministic and is
rebuilt and validated by `make check` in CI, which asserts the timing-ready count: if it
ever returns to zero, the device cannot arm at any real venue again.

## 6. On-card layout and device loading

Track geometry lives on the microSD card. Nothing is built into the firmware, and
adding a track needs no firmware rebuild.

```text
/sdcard/track-packs/<pack_id>/definitions/<track_id>.json
```

This is exactly what `tools/build_uk_track_pack.py` emits, so a pack can be copied to
the card unchanged. Several packs may coexist; `device-captured` is reserved for
geometry captured on the device itself, so wiping a distributed pack never deletes a
capture. Packs are only recognised at that path: a directory is scanned only if it
contains a `definitions` subdirectory.

At boot `SdTrackStore` enumerates every pack and `catalog::TrackCatalog` parses each
definition into a caller-owned array. Two allocations dominate and both are placed in
PSRAM rather than internal RAM or a task stack:

| Allocation | Size | Why |
|---|---:|---|
| Catalog array | `TrackDefinition` is 3.6 KB, so 32 entries is ~115 KB | Feeds `TrackCatalogView` directly, reusing the tested matching logic |
| Load scratch | `TrackDefinitionBlob` is 16 KB plus one 3.6 KB definition | A file blob must never sit on a task stack |

Parsing is also stack-hungry: `load_track_definition` holds a full `TrackDefinition` in
its parse state, so any task that loads a track needs roughly 10 KB of stack headroom.

### Selecting a track

`catalog::apply_track` performs the sequence, and its ordering is deliberate:

```text
read from card -> parse -> check timing readiness -> make_timing_engine_config -> configure
```

The definition is read, parsed and checked **before** the timing engine is touched, so a
missing card, an unreadable file or corrupt geometry leaves a previously loaded track
running untouched. `TimingEngine::configure()` resets on every failure path, so the
engine is never left partially configured. Provisional geometry is refused outright and
stays timer-only.

### Degradation

No card, an unreadable card, or no pack directory degrades to timer-only operation and
the session countdown is unaffected, satisfying the rule that SD failure must not stop
timing. Once a track is loaded its geometry is resident in device memory, so removing
the card mid-session does not disturb timing.

A corrupt definition is counted as rejected and skipped rather than aborting the build,
so one bad file cannot hide an entire card of valid tracks. If a card holds more
definitions than the catalog can accommodate, the overflow is reported rather than
silently truncated.

## 7. Loading, projection, and versioning

Every track file includes `schema_version`; the current loader accepts exactly version 3.
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

Versions 1 and 2 are not inferred or silently upgraded. Version 1 contains only one line
and cannot safely invent pit entry/exit geometry; version 2 has no geometry-validation
status and therefore cannot prove timing readiness. A reviewed migration must supply all
four gates, safety parameters, minimum lap time, and an explicit validation status.

The firmware-facing contract is `track_timer/track/definition.hpp`. File-system and
NVS/SD-card adapters remain outside the parser, so malformed storage input cannot gain
ownership of timing state.
