# Data Logging

## 1. Purpose

Logging is not only for post-session entertainment. It is how timing errors are investigated and how the embedded result is reproduced off-device.

## 2. Session directory

Suggested structure:

```text
/sessions/2026-08-12T140501Z/
    meta.json
    gnss.csv
    events.csv
    summary.json
```

Binary logging may be introduced later if CSV write cost is unacceptable. Start with a format that is easy to inspect.

## 3. `meta.json`

Include:

- firmware version / Git commit
- hardware profile
- GNSS model and configuration
- configured update rate
- track ID
- track schema version, identifier, and 16-character exact-file fingerprint
- session settings
- start/end UTC if available
- reset reason at boot

## 4. GNSS rows

Suggested fields:

```text
seq
gnss_time_ns
arrival_monotonic_us
lat_deg
lon_deg
speed_mps
heading_deg
h_acc_m
speed_acc_mps
heading_acc_deg
fix_type
num_sv
accepted_for_timing
reject_reason
```

## 5. Event rows

Suggested fields:

```text
event_type
lap_index
gnss_time_ns
lap_time_ms
segment_seq0
segment_seq1
intersection_fraction
quality_flags
```

## 6. Write strategy

- producers enqueue compact records
- one logger task serializes/writes
- use buffered writes
- flush periodically and at state transitions
- do not `fsync` on every GNSS fix
- count queue overflow and write errors

## 7. Power-loss tolerance

A sudden power loss may truncate the final row/file. Design replay tools to tolerate an incomplete final record.

Session summary should be derivable from events rather than being the only source of truth.

## 8. Privacy

Track logs contain precise location history. Default behaviour should keep logs local. Any future wireless export must be user-initiated and documented.
