# Versioned settings

Release-one settings use a fixed-size, checksummed record behind the
`SettingsStore` interface. The UI, timing path, and settings model do not perform direct
file or NVS operations.

## Current schema

Version 4 stores:

- session and rest durations
- launch sensitivity
- average lap time and lower-display choice
- day and night brightness presets
- timer/G-meter operating mode
- Trackday Mode enable/disable state
- repeating lap boundary (`START LINE` or `FINISH LINE`)
- independent Pit Exit Auto-Start and Pit Entry Auto-Stop enable/disable states
- fixed or automatic orientation
- stationary auto-dim preference
- selected local track identifier

The record has a magic value, explicit version, payload length, and payload checksum.
Fields have fixed widths and little-endian encoding; compiler structure padding is never
persisted.

## Loading and migration

- A valid version 4 record loads directly.
- Versions 1, 2, and 3 are migrated explicitly, with new fields receiving documented safe
  defaults, then rewritten atomically. Trackday Mode and both pit automations default to
  disabled during migration; the lap boundary defaults to Finish.
- Missing, corrupt, invalid, or unsupported records are never reinterpreted. Safe
  defaults are used and a current record is written when storage permits.
- Storage read failure keeps safe defaults in memory without claiming they were saved.

The simulator file adapter writes a temporary file, preserves the previous file as a
backup during replacement, and recovers that backup if a replacement was interrupted.
The interactive simulator places that record at
`/tmp/track-session-timer-simulator/settings-v2.bin` on Linux so normal use cannot
dirty the repository. The filename remains stable for backward-compatible discovery;
the record itself carries the authoritative schema version.
The hardware adapter can provide the same contract through NVS once the board arrives.

## On-device editor contract

Setup presents all version 4 options through one consistent field/value editor. Track
and rest duration retain the established 1, 5, 10, 15, 20, 25, 30, 40, 50, and 60
minute choices; launch sensitivity and brightness use their supported discrete values.
Average lap time is second-precise from `00:00` through `59:59`. Selecting `LAPS LEFT`
without an average lap is rejected visibly, and clearing the average automatically
restores `COUNT UP`.

Trackday Mode is an explicit `ENABLED`/`DISABLED` setting and defaults to disabled.
The effective value is frozen when a session starts. When enabled, active presentation
is limited to session countdown and estimated laps remaining; lap detection and logging
continue, with lap results available through Review only after the session stops.

Lap Line explicitly selects Start or Finish as the repeating lap boundary. Pit Exit
Start and Pit Entry Stop are independent and default to disabled. When enabled, only an
accepted, directed, debounced typed gate event can act: pit exit starts from Ready, and
pit entry stops from Running or Overtime with `PIT ENTRY` recorded as the completion
reason. Rejected, duplicate, disabled, and wrong-state events are no-ops. Manual Start
and the guarded manual Stop flow remain available regardless of these settings.

Edits remain a draft until Save. Cancel discards the complete draft, and Restore
Defaults requires a separate confirmation before staging defaults for Save. Restoring
device defaults deliberately preserves the selected track because track ownership and
selection are handled by the track workflow.

Brightness and orientation drafts are previewed through the same display-policy path
used by saved settings. Save makes the preview the effective policy; Cancel and a failed
save immediately restore the persisted policy. No preview writes directly to a display
driver.

## Display policy

Day and night select their corresponding brightness presets. Optional auto-dim applies
only after 60 seconds of stationary Ready-screen inactivity and wakes on the next input.
The minimum dimmed policy is the supported 25% preset. A bounded eight-position pattern
moves stationary Ready content by at most four pixels every 30 seconds to reduce static
AMOLED exposure.

Fixed orientations map directly to 0, 90, 180, or 270 degrees. Automatic orientation
uses a valid sensed orientation and retains the prior value when no sensor result is
available. Brightness, orientation, dimming, and layout movement are frozen safely on
entry to active timing: active content never auto-dims, shifts, or rotates. A changed
automatic orientation is reported as deferred and becomes effective after timing ends.

The policy produces a fixed-size `board::DisplayCommand`; only a board adapter may turn
that command into panel brightness or rotation operations. Diagnostics reports the
effective brightness, orientation, dim state, and layout offset.

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
