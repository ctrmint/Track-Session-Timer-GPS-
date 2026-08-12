# UI and UX Requirements

## 1. Design objective

The device is read while the driver is busy. The UI is not a miniature phone interface.

The primary driver screen should answer, in under one glance:

1. how much session time is left?
2. what is the current lap time?
3. was the last lap faster or slower?
4. is GPS healthy?

## 2. Active screen concept

Example only:

```text
+--------------------------------------------------+
|  LAP 07                         GPS GOOD  18 SV  |
|                                                  |
|                    1:42.638                      |
|                                                  |
|                  -0.347 DELTA                    |
|                                                  |
|  LAST 1:43.112          BEST 1:42.985            |
|                                                  |
|             SESSION  15:27                       |
+--------------------------------------------------+
```

For the first release, live delta may be omitted and the current lap/session values made larger.

## 3. Session colour

Preserve the original TrackSessionTimer strength: the whole visual environment indicates session progression.

Suggested progression:

- early session: green
- mid session: yellow
- later session: amber
- final warning: red
- overtime: purple

Use contrast-calculated text. Colour must not be the only way a critical state is communicated.

## 4. GNSS indicator

Do not show a meaningless generic satellite icon alone.

States:

- `NO GPS`: receiver unavailable
- `SEARCH`: receiver active, no valid fix
- `POOR`: fix valid but quality outside timing threshold
- `GOOD`: acceptable for lap timing
- `STALE`: no fresh fix within expected interval

Satellite count and horizontal accuracy can appear on a diagnostic/setup screen. The active screen should stay simple.

## 5. Lap feedback

When a lap completes, briefly emphasise:

- completed lap time
- faster/slower relative to best

Do not cover session time for several seconds. A 1 to 2 second transient is a starting point for user testing.

## 6. Touch safety

During active movement:

- no small buttons
- no nested menus
- no gesture that can reset timing accidentally
- destructive action requires an intentional hold/confirmation
- settings disabled or deferred while lap timing is active

## 7. Brightness

Prototype at maximum brightness in direct daylight. Later add:

- day preset
- night preset
- optional auto-dim while stationary/ready

Do not auto-dim during an active session without a specific tested rule.

## 8. Orientation

The existing project's automatic/fixed orientation concept can be retained, but active-session rotation should not create a frame or input stall. Fixed installation is preferable for the first track prototype.

## 9. Screen burn-in

AMOLED static UI elements require sensible design. During long ready/idle periods use dimming or minor layout movement as appropriate. Active track sessions are dynamic enough that readability remains the first priority.
