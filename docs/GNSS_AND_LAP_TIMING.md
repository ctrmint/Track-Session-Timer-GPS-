# GNSS and Lap Timing Design

## 1. Goal

Produce repeatable track-day lap events from a 20/25 Hz standard-precision GNSS receiver without pretending that update rate alone equals timing accuracy.

The design separates:

- receiver update rate
- GNSS position accuracy
- GNSS time accuracy
- start-line geometry
- event interpolation
- filtering/validation

## 2. Receiver configuration

Baseline NEO-M9N target:

- 20 Hz during initial development
- evaluate 25 Hz after stable logging
- automotive/dynamic platform model where supported and appropriate
- UBX binary protocol
- only required messages enabled at high rate
- GNSS time kept with each fix
- fix type and accuracy estimates preserved

The configured update rate must be read back or inferred from observed message cadence. Do not assume a configuration command succeeded.

## 3. Internal fix model

Each accepted GNSS fix should contain at least:

```text
measurement_time
arrival_monotonic_us
latitude_deg
longitude_deg
height_m             optional for lap timing
speed_mps
heading_deg
horizontal_accuracy_m
speed_accuracy_mps
heading_accuracy_deg
fix_type
num_satellites
valid_flags
sequence_number
```

Keep raw receiver status available for diagnostics even if the timing engine uses a smaller validated subset.

## 4. Coordinate system

Lap intersection calculations should not operate directly on latitude/longitude degrees.

For a local circuit, convert positions and start-line endpoints to a local tangent plane in metres, for example East/North coordinates around a track reference origin.

A simple equirectangular/local tangent approximation is adequate for small test geometry, but production code should use a well-tested local projection implementation with explicit numerical tests.

## 5. Start/finish definition

A track file stores:

- endpoint A
- endpoint B
- permitted crossing direction
- optional line width metadata for display only
- minimum plausible lap time
- broad track geofence

Direction can be represented as a unit normal vector or a heading range associated with the line.

## 6. Segment crossing

For consecutive valid fixes `P0` and `P1`, determine whether the movement segment intersects the start line segment `A` to `B`.

A robust 2D segment intersection method returns a movement fraction `u` where:

```text
crossing_position = P0 + u * (P1 - P0)
0 <= u <= 1
```

If no valid segment intersection exists, no lap event is possible.

## 7. Time interpolation

Let GNSS measurement times be `T0` and `T1`.

```text
Tcross = T0 + u * (T1 - T0)
```

At 25 Hz, adjacent nominal fixes are 40 ms apart. Interpolation removes update-grid quantisation, but does not eliminate position noise or path curvature between fixes.

## 8. Direction check

A geometric intersection is accepted only when movement crosses the line in the configured direction.

Possible implementation:

1. create a signed side value of each position relative to the directed start line
2. require a side transition consistent with track direction
3. require vehicle speed above a minimum
4. optionally compare GNSS heading with expected crossing heading

Direction is essential for circuits where pit lanes, access roads or nearby loops pass close to the line.

## 9. Hysteresis and duplicate suppression

Use several protections together:

- minimum lap time per track
- must move a defined distance or leave a line corridor before rearming
- valid direction only
- minimum speed
- GNSS quality threshold
- sequence monotonicity

Do not rely on a single time debounce.

## 10. Fix quality

The timing engine should be able to reject or mark suspect fixes based on:

- invalid fix status
- implausible timestamp jump
- horizontal accuracy above configured threshold
- impossible speed/position discontinuity
- stale fix age

Avoid over-filtering positions in a way that adds variable time lag to line crossing. If smoothing is used for UI position or heading, keep the raw accepted fixes available to the lap event engine unless testing proves a better approach.

## 11. GNSS time versus UART arrival

UART arrival time includes receiver processing and serial transport delay. It can vary as message load changes.

Use the receiver's measurement timestamp for lap crossing. Store MCU arrival time separately to detect latency and queueing problems.

PPS/timepulse can be used to characterise the relationship between GNSS time and the ESP32 monotonic clock, but the MVP does not require PPS to calculate relative lap durations when both crossings use GNSS measurement time.

## 12. Lap state machine

Suggested states:

```text
NO_TRACK
WAITING_FOR_FIX
ARMED
LAP_RUNNING
CROSSING_CANDIDATE
LAP_COMPLETE
REARM_WAIT
```

The actual implementation can be simpler, but state transitions must be explicit and unit tested.

## 13. Start behaviour

Do not count a lap immediately on boot if the car is physically sitting on the start line.

Possible MVP rule:

- acquire valid track and GNSS fix
- require device to be outside the start-line rearm corridor
- arm timing
- first valid directed crossing starts Lap 1
- next crossing completes Lap 1

Track-day workflow may later allow a flying first lap mode.

## 14. Recorded trace replay

Every track test should produce enough data for an offline tool to replay:

- accepted/rejected fixes
- line intersection fraction
- direction result
- rearm state
- lap-event timestamp

The embedded result and host replay should be identical within floating-point/rounding tolerance.

## 15. Live delta, later phase

Live delta is not simply current elapsed time minus best lap elapsed time. It requires a reference mapping between position/distance along the circuit and elapsed time.

Possible later approach:

1. create a best-lap polyline/reference trace
2. calculate cumulative path distance
3. project current position onto a nearby reference segment
4. interpolate best-lap elapsed time at that projected progress
5. delta = current lap elapsed - reference elapsed

This needs robust off-track handling and should not be part of the first timing MVP.
