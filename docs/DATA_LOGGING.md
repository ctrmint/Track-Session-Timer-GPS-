# Data Logging

## 1. Purpose

Logging is not only for post-session entertainment. It is how timing errors are investigated and how the embedded result is reproduced off-device.

## 2. Session directory

Version 1 uses this structure:

```text
/sessions/2026-08-12T140501Z/
    meta.json
    gnss.csv
    events.csv
    summary.json
    summary_laps.csv
```

Every file declares `schema_version=1`. JSON uses a numeric `schema_version` member;
CSV repeats it as the first column of every row so a fragment remains identifiable.
Binary logging may be introduced later if measured CSV write cost is unacceptable,
but it must preserve the same semantic fields and version boundary.

## 3. `meta.json`

The version 1 metadata object contains:

| Key | Type and unit | Requirement |
|---|---|---|
| `schema_version` | unsigned integer | exactly `1` |
| `session_id` | string, max 31 bytes | unique directory/session identifier |
| `firmware_commit` | string, max 40 bytes | full Git commit |
| `hardware_profile` | string, max 31 bytes | selected board/profile identifier |
| `gnss_profile` | string, max 31 bytes | receiver and configuration profile |
| `gnss_update_rate_hz` | unsigned integer, Hz | configured rate |
| `reset_reason` | enum string | `unknown`, `power_on`, `software`, `watchdog`, `brownout`, or `panic` |
| `track_schema_version` | unsigned integer | zero for timer-only, otherwise source track version |
| `track_id` | string, max 47 bytes | empty for timer-only |
| `track_fingerprint` | 16 lower-case hex characters | exact-file FNV-1a fingerprint; empty for timer-only |
| `start_utc_ns`, `end_utc_ns` | signed integer, UTC nanoseconds | `-1` when unavailable |
| `settings` | object | complete versioned `DeviceSettings` snapshot |

The settings object records the settings schema version, session/rest minutes,
launch sensitivity in milli-g, average lap seconds, day/night brightness percentages,
operating mode, orientation, auto-dim flag, lower-display mode, and selected track ID.
This is a copy captured at session start; replay never consults current device settings.

## 4. GNSS rows

The exact version 1 header is:

```text
schema_version,record_sequence,measurement_time_ns,arrival_monotonic_us,latitude_deg,longitude_deg,height_m,speed_mps,heading_deg,horizontal_accuracy_m,speed_accuracy_mps,heading_accuracy_deg,fix_sequence,valid_flags,num_satellites,fix_type,accepted_for_timing,reject_reason
```

Angles are degrees, distances are metres, speeds are metres per second, GNSS time is
nanoseconds, and monotonic arrival time is microseconds. `record_sequence` orders all
source records across the session; `fix_sequence` is the receiver/parser sequence.
Every fix is recorded. An accepted fix has `reject_reason=none`; every rejected fix
must carry a non-`none` reason. This makes both accepted and rejected timing inputs
replayable.

## 5. Event rows

The exact version 1 header is:

```text
schema_version,record_sequence,ordering_monotonic_us,event_type,lap_index,measurement_time_ns,lap_duration_ns,segment_sequence_0,segment_sequence_1,intersection_fraction,quality_flags,session_elapsed_ms,session_overrun_ms
```

Event types are `session_started`, `lap_crossing`, `overtime_started`,
`session_stopped`, `rest_started`, `rest_completed`, and `diagnostic`. A lap crossing
must identify the two surrounding GNSS `fix_sequence` values and an interpolation
fraction from 0 through 1. `quality_flags` is the exact timing-quality bitset captured
at the decision. Durations are nanoseconds; session elapsed/overrun are milliseconds.

## 6. Derived summary

`summary.json` is a bounded aggregate, not the source of truth. Version 1 contains the
session ID, total duration and overrun in milliseconds, completion reason, integrity
(`complete` or `partial_log`), degraded-subsystem flags, lap count, best lap index and
duration, first/last source record sequence, accepted/rejected/total GNSS counts, event
count, logger drop count, and logger write-failure count.

Lap rows are stored separately in pageable `summary_laps.csv`:

```text
schema_version,lap_index,lap_duration_ns,event_record_sequence,segment_sequence_0,segment_sequence_1,quality_flags
```

`event_record_sequence` links each derived lap to its source event. The segment IDs in
that event link to the surrounding GNSS rows. A replay regenerates the summary by
ordering source records by `record_sequence`, counting every GNSS acceptance decision,
collecting valid lap events, selecting the minimum lap duration, and copying terminal
session/degradation/logger counters. A generated summary is `partial_log` if the source
sequence has gaps, the logger reports drops/write failures, or the final row was
truncated. The source CSV files always win if a stored summary disagrees.

The firmware-facing definitions and validators are in
`track_timer/logger/formats.hpp`. They use fixed-capacity, trivially-copyable records;
serializers must write named fields and must never dump native struct bytes.

## 7. Write strategy

`AsyncLogger` is the only owner of `StorageBackend` during normal operation. GNSS,
timing, and session producers receive only its non-blocking `enqueue` and
`request_flush` operations; they cannot call storage. A short try-lock protects the
fixed ring. Contention drops instead of blocking a timing producer and is counted
separately from capacity overflow.

Version 1 budgets are:

| Item | Budget/policy |
|---|---|
| producer queue | 256 compact `LogRecord` values (at least 10.24 s at 25 Hz before event overhead) |
| writer batch | up to 16 records per storage call |
| logger task | 4096-byte stack, priority 5, 10 ms service period |
| periodic drain | at most 250 ms between partial-batch writes |
| explicit flush | session state transition, stop, and orderly shutdown |

The ESP-IDF `session_logger` task owns `service()`. It copies one bounded batch out of
the producer ring before any storage call, so enqueue never waits for SD latency. A
failed or unavailable write retains that staged batch for retry while new records can
continue filling the main queue. Storage batch failure must be atomic: `false` means no
record in that batch was accepted. Periodic draining writes into the storage backend's
buffer; explicit flush completes that buffer but must not imply per-fix `fsync`.

Metrics expose accepted/written records, depth/high-water, full and contention drops,
invalid records, batch count/size, unavailable-storage attempts, failed write attempts,
flush requests/completions, maximum latency, average latency, and bounded-histogram
p50/p95/p99 estimates. The simulator prints the operational subset in its final status
line. Deterministic stress runs cover 30 simulated minutes at both 20 Hz and 25 Hz on a
40 ms display cadence with zero producer drops; missing/full/slow/write-failed storage
remains separately fault-injectable. Physical SD contention and durability still need
confirmation on the target board.

## 8. Power-loss tolerance

A sudden power loss may truncate the final row/file. Design replay tools to tolerate an incomplete final record.

Session summary should be derivable from events rather than being the only source of truth.

## 9. Versioning and migration

Version 1 field names, meanings, units, enum spellings, and CSV column order are
immutable. Readers reject unknown major versions instead of guessing. A future version
may add files or columns only under a new schema number, with an explicit converter and
golden replay fixtures. Migration is performed on a host copy; original session files
remain unchanged so timing evidence is never silently rewritten. Incomplete final CSV
rows may be ignored, but an invalid row in the middle marks the session partial/corrupt
and is not skipped silently.

## 10. Privacy

Track logs contain precise location history. Default behaviour should keep logs local. Any future wireless export must be user-initiated and documented.
