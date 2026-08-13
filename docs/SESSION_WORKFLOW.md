# Session workflow

The session controller is a deterministic, hardware-independent state machine driven
only by monotonic milliseconds and explicit user events.

```text
READY <---- cancel/save ---- CONFIGURING
  |
  | start
  v
RUNNING ---- scheduled duration ----> OVERTIME
  |                                  |
  +---- confirmed stop --------------+
                    |
                    v
                  REVIEW ---- no rest configured ----> READY
                    |
                    | review complete
                    v
                   REST ---- duration or confirmed skip ----> READY
```

Stop confirmation is a guarded sub-state of running, overtime, or rest. Time continues
while confirmation is visible, short or accidental input can cancel it, and only a
separate confirmation event ends the active phase. GNSS, storage, IMU, display refresh,
and wall-clock time are deliberately absent from the controller contract.

The UI synchronizes directly from controller snapshots. Overtime keeps the normal
timing display but replaces the countdown with a signed overrun and an `OVERTIME` text
heading. Confirmation keeps that live time visible. Review reports the frozen
completion reason, and Rest uses its own `REST / RECOVERY` screen with a fixed-cell
countdown. Skipping rest repeats the same hold, release, and confirmation safeguard;
ordinary taps, gestures, or Back cannot complete either timed state.

## Configuration invariants

- Session duration is between one minute and 24 hours.
- Rest duration is between zero and 24 hours; zero skips rest.
- Configuration can be saved or cancelled only from `CONFIGURING`.
- Configuration cannot be entered or changed while running, in overtime, reviewing,
  or resting.
- Backwards or negative monotonic timestamps are rejected without changing state.

## Differences from the original timer

- Configuration has explicit staged save/cancel semantics instead of writing during
  gesture handling.
- Stop confirmation is represented independently from the timed state so countdowns
  continue while the confirmation UI is visible.
- Review persistence and page content are separate bounded interfaces; the controller
  owns only lifecycle and frozen timing values.
- A zero rest duration explicitly returns to Ready after review.
- Launch detection, G-meter behavior, display rotation, and touch gestures are separate
  policies that send events to this state machine rather than controlling time directly.
