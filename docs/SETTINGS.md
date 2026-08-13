# Versioned settings

Release-one settings use a fixed-size, checksummed record behind the
`SettingsStore` interface. The UI, timing path, and settings model do not perform direct
file or NVS operations.

## Current schema

Version 2 stores:

- session and rest durations
- launch sensitivity
- average lap time and lower-display choice
- day and night brightness presets
- timer/G-meter operating mode
- fixed or automatic orientation
- stationary auto-dim preference
- selected local track identifier

The record has a magic value, explicit version, payload length, and payload checksum.
Fields have fixed widths and little-endian encoding; compiler structure padding is never
persisted.

## Loading and migration

- A valid version 2 record loads directly.
- Version 1 is migrated explicitly, with new fields receiving documented version 2
  defaults, then rewritten atomically.
- Missing, corrupt, invalid, or unsupported records are never reinterpreted. Safe
  defaults are used and a current record is written when storage permits.
- Storage read failure keeps safe defaults in memory without claiming they were saved.

The simulator file adapter writes a temporary file, preserves the previous file as a
backup during replacement, and recovers that backup if a replacement was interrupted.
The hardware adapter can provide the same contract through NVS once the board arrives.

## Active-session safety

Valid settings submitted while a session is active replace one bounded pending value;
they do not change effective settings or write storage. The pending value can be applied
only after active timing ends. Invalid values and failed writes leave effective settings
unchanged.

## Degraded operation

Subsystem state is converted to feature availability without disabling the monotonic
session timer:

- GNSS unavailable or degraded disables lap timing.
- Storage unavailable disables logging; degraded storage remains usable with a visible
  warning.
- IMU unavailable disables G-meter features.
- Touch unavailable disables touch control but not countdown progression.
- RTC unavailable removes wall-clock metadata but not monotonic timing.
