# Application navigation

The hardware-independent UI starts at the ready dashboard and has five top-level
destinations:

```text
                         +--> Setup --> Track Selection --+
                         |          +-> Device Settings --+--> Setup --> Back --> Ready
Ready -- Start --> Active+--> Review ---------------------+--> Back --> Ready
                         +--> Diagnostics ----------------+
```

Start changes the navigation controller to `active` and emits a single start request
for the session service. While active, Setup, Review, Diagnostics, and Back actions
are rejected. A completed session or service synchronization returns navigation to
Ready. This makes timing authority explicit and prevents a UI gesture from bypassing
the session state.

Pointer clicks, touchscreen taps, keyboard activation, and injected abstract actions
all dispatch `ui::NavigationAction`; there is no separate simulator-only navigation
logic. Host tests exercise every accepted return path and every active-session guard.

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
capture is identified separately and remains bounded to issue #37. Review and
Diagnostics remain returnable destination shells whose detailed content is bounded to
issues #80 and #81.
