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

The production projection is `CircuitProjection`, a WGS84 ellipsoid local tangent
linearisation anchored at the track file's `reference` point. It precomputes the WGS84
prime-vertical and meridional radii at that latitude, then maps every accepted fix and
all gate endpoints to:

```text
east_m  = normalised_longitude_delta_rad * N(reference) * cos(reference_latitude)
north_m = latitude_delta_rad             * M(reference)
```

Longitude delta is normalised across the antimeridian. East is positive with increasing
longitude and North is positive with increasing latitude. Inputs and scale factors use
`double`; outputs are metres relative to the reference origin. The same allocation-free
C++ implementation is compiled for host replay, the simulator, and ESP32-S3 firmware.

The bounded circuit projection accepts reference latitudes from -85 to +85 degrees and
points within 25 km of the origin. It is deliberately a local geometry projection, not
a global distance service. Invalid, unconfigured, polar, non-finite, or out-of-area
inputs are rejected without changing the caller's previous output.

Reference vectors enforced to 1 mm by `test_projection.cpp` include:

| Reference | Offset | East (m) | North (m) |
|---|---|---:|---:|
| 0°, 0° | +0.1° longitude | 11,131.949 | 0.000 |
| 0°, 0° | +0.1° latitude | 0.000 | 11,057.428 |
| 52°, -1° | +0.001° longitude, +0.001° latitude | 68.678 | 111.267 |
| -33.9°, 151.2° | -0.001° longitude, +0.001° latitude | -92.493 | 110.921 |
| 0°, 179.999° | longitude -179.999° | 222.639 | 0.000 |

Projection configuration runs only when a track is loaded and performs the WGS84
trigonometry and square root once. The per-fix path contains no trigonometry, square
root, allocation, or I/O: it normalises longitude, performs fixed-scale arithmetic, and
checks squared radius. This cost is bounded independently of catalog size and sampling
rate. With the pinned ESP-IDF 6.0.2 ESP32-S3 toolchain, `projection.cpp.obj` measures
1,416 bytes of text and zero bytes of data/BSS. End-to-end deadline measurement on the
target timing task remains part of embedded parity issue #25.

## 5. Circuit gate definitions

A track file stores four directed gates (`start`, `finish`, `pit_entry`, and `pit_exit`).
Each gate stores:

- left endpoint
- right endpoint
- permitted crossing direction
- minimum crossing speed
- rearm corridor distance

The track also stores a minimum plausible lap time and broad geofence.

Direction can be represented as a unit normal vector or a heading range associated with the line.

## 6. Segment crossing

For consecutive valid fixes `P0` and `P1`, determine whether the movement segment intersects the start line segment `A` to `B`.

A robust 2D segment intersection method returns a movement fraction `u` where:

```text
crossing_position = P0 + u * (P1 - P0)
0 < u <= 1
```

Consecutive movement segments use the half-open interval `(P0, P1]`: the current
segment owns a crossing at its ending fix, while the next segment does not own
the same point at its starting fix. The timing line uses the closed interval
`[A, B]`, so crossing either physical endpoint still counts. A
before/on/after sequence therefore produces exactly one geometric candidate.

Zero-length movement and timing-line segments are rejected with distinct
diagnostics. Parallel segments do not cross, and collinear segments are
reported as ambiguous rather than creating a timing event. The implementation
normalises its parallel tolerance by both segment lengths and derives endpoint
parameter tolerances from segment length, so decisions remain consistent at
different circuit-local scales. Repeated fixes on the line are rejected as
degenerate movement.

If no owned, unique segment intersection exists, no lap event is possible.

## 7. Time interpolation

Let GNSS measurement times be `T0` and `T1`.

```text
Tcross = T0 + u * (T1 - T0)
```

At 25 Hz, adjacent nominal fixes are 40 ms apart. Interpolation removes update-grid quantisation, but does not eliminate position noise or path curvature between fixes.

Internally, measurement time is a continuous signed 64-bit count of receiver
nanoseconds; `-1` means unavailable. A receiver's modulo-week time is normalised
before a fix enters the timing engine. A backward jump of more than half a GPS
week is the only transition treated as a forward rollover. Equal/small backward
jumps and forward jumps over half a period are explicit discontinuities and do
not mutate normaliser state.

The default pair policy accepts a maximum 500 ms measurement gap and a current
fix no more than 500 ms old. It also requires increasing fix sequences,
measurement timestamps, and monotonic arrival timestamps. Interpolation rounds
to the nearest nanosecond after scaling the small pair interval, avoiding the
precision loss of converting a full epoch-sized timestamp to floating point.
Every failure has a stable name and records source sequences, fraction, interval,
age, and the applied policy for deterministic replay.

## 8. Direction check

A geometric intersection is accepted only when movement crosses the line in the configured direction.

The production validator:

1. computes signed perpendicular distances for both positions relative to the gate
2. orients those distances using the gate's configured travel heading
3. requires a transition from the negative side to the line or positive side
4. interpolates speed and heading at the geometric intersection fraction
5. requires the interpolated speed and shortest-arc heading difference to meet the
   gate thresholds

Heading interpolation and comparison use the shortest circular difference, so
`359` degrees to `1` degree is a two-degree change rather than 358 degrees.

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

Both source fixes for a crossing must already be accepted for timing. The default
crossing thresholds require a 3D fix, at least 6 satellites, horizontal accuracy
no worse than 5 m, speed accuracy no worse than 2 m/s, and heading accuracy no
worse than 25 degrees. These defaults are explicit configuration values rather
than hidden constants; a track's gate separately defines minimum crossing speed
and permitted heading tolerance. A 2D-fix policy can be selected explicitly, but
dead-reckoning and time-only fixes are never silently treated as position fixes.

Each geometric candidate produces one stable named validation result and records
the two source sequence numbers, intersection fraction, signed distances,
interpolated speed and heading, and heading difference. Together with the raw
GNSS rows, the applied quality-threshold snapshot, and fingerprinted track
definition, this makes every acceptance or rejection reproducible during
offline replay.

Avoid over-filtering positions in a way that adds variable time lag to line crossing. If smoothing is used for UI position or heading, keep the raw accepted fixes available to the lap event engine unless testing proves a better approach.

## 11. GNSS time versus UART arrival

UART arrival time includes receiver processing and serial transport delay. It can vary as message load changes.

Use the receiver's measurement timestamp for lap crossing. Store MCU arrival time separately to detect latency and queueing problems. Arrival time participates only in ordering and staleness checks; changing UART latency within those limits cannot change the calculated crossing timestamp.

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

The timing component implements explicit `no-track`, `waiting-for-fix`, `armed`,
`lap-running`, `lap-complete`, and `rearm-wait` states. A valid fix must first be
outside the configured gate corridor before the engine arms. The first accepted
crossing starts Lap 1; subsequent crossings complete the current lap and start
the next one at the same receiver timestamp.

After every crossing candidate, including a candidate rejected by the minimum
lap-time guard, the engine requires another corridor exit. This makes spatial
hysteresis the primary duplicate suppression mechanism rather than relying on a
time delay alone. A below-minimum candidate marks the current lap suspect for
replay, and suspect completed laps are retained but excluded from the best-lap
calculation.

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
