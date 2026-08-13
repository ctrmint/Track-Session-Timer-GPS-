# Application navigation

The hardware-independent UI starts at the ready dashboard and has six top-level
destinations:

```text
                         +--> Setup --> Track Selection --+
                         |          +-> Device Settings --+--> Setup --> Back --> Ready
                         |          +-> G-meter / IMU -----+
Ready -- Start --> Active+--> Review -- Ready ------------+--> Ready
                         |          +-> Rest -- expiry/skip+--> Ready
                         +--> Diagnostics ----------------+
```

Start changes the navigation controller to `active` and emits a single start request
for the session service. While active, Setup, Review, Diagnostics, and Back actions
are rejected. A completed session or service synchronization returns navigation to
Ready or advances it to the dedicated Rest destination. This makes timing authority
explicit and prevents a UI gesture from bypassing the session state.

Pointer clicks, touchscreen taps, keyboard activation, and injected abstract actions
all dispatch `ui::NavigationAction`; there is no separate simulator-only navigation
logic. Host tests exercise every accepted return path and every active-session guard.

## Active session

When a lap completes, the current-lap region shows the completed duration and its
faster/slower comparison for a deterministic 1.8 seconds. A first valid lap states
`BEST ESTABLISHED` instead of inventing a comparison. The session countdown remains
in its own persistent panel throughout the feedback interval, and another completed
lap replaces the prior feedback with a fresh bounded interval.

Stopping is deliberately separate from ordinary navigation: the driver must hold the
Stop control for 1.5 seconds, release it, and then choose Stop on the confirmation
panel within five seconds. Short holds, press-loss events, Cancel, and confirmation
timeouts return to timing without emitting a stop request. Setup, Review, Diagnostics,
Back, taps, and swipes remain unable to end or leave an active session.

## Ready dashboard

The dashboard shows:

- selected track;
- session and rest duration;
- GNSS, storage, IMU, and logging readiness using both text and colour;
- `LAP TIMING READY` when GNSS is suitable;
- an explicit `TIMER ONLY` reason when GNSS is unavailable or poor.

Start remains usable in the degraded timer-only path. Setup is disabled/deferred once
a session is active. Interactive controls meet the shared 56 pixel minimum target.

Setup is a returnable menu. Device Settings provides explicit Save, Cancel, validation,
and confirmed Defaults behavior. Track Selection lists the validated local catalog,
shows start/finish readiness, persists confirmed selections through the shared settings
manager, and provides a timer-only choice. Suggested tracks require confirmation;
ambiguous, missing, invalid, and unavailable states are explained without inventing a
lap-timing state. Track changes are rejected while a session is active. Unknown-track
capture is identified separately and remains bounded to issue #37.

G-meter / IMU shows current planar acceleration, a fixed 24-sample trail, the session
peak marker, and acceleration/braking/left/right/total peaks. Its labels identify the
physical axes for the effective configured orientation. Calibration, partial data,
unavailable data, and the bounded recovered notice remain distinct. Reset Peaks is
accepted only outside active timing; measurements also reset automatically as timing
starts and remain available when it ends. Losing IMU data never changes session
navigation or timing state.

## Session review

Review shows a bounded session aggregate: total duration, overrun, completion reason,
log integrity, degraded subsystems, and four lap rows at a time. Newer/Older browse
completed sessions and the lap controls page through longer sessions. Best and
previous laps use text plus colour emphasis. Empty history, partial logs, unavailable
storage, corrupt summaries, and unsupported summary versions remain distinct states.

The screen reads only the `SessionSummaryProvider` contract; it never opens session
files or loads a GNSS trace. REST and READY emit separate handoff requests to the
session service, which advances to Rest or Ready respectively. Deterministic simulator
fixtures exercise all summary states while physical storage is unavailable.

## Rest

Rest presents the remaining recovery time, the prior completion reason, and the
automatic next step without relying on colour. It exits automatically at zero. An
early skip requires a 1.5-second hold, release, and explicit confirmation; short
presses, cancelled input, taps, gestures, and Back do not change lifecycle state.
The countdown is monotonic and independent of GNSS, IMU, and storage availability.

## Diagnostics

Diagnostics is available only while stationary and pages through System, GNSS,
Logging, and Peripheral values. It consumes one immutable snapshot, so LVGL never
queries a driver or queue directly. Normal, degraded, missing, and recovered states
use text and colour cues; historical drop/failure/recovery counters remain visible
after recovery. Back always returns to Ready, and the navigation guard rejects the
destination while timing is active.
