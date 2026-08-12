# Test Plan

## 1. Test pyramid

### Host unit tests

Fast, deterministic tests for:

- coordinate conversion
- segment intersection
- direction classification
- time interpolation
- minimum lap time
- rearm/hysteresis
- timestamp rollover/ordering assumptions
- track schema validation

### Host replay tests

Recorded/synthetic traces with expected lap events.

### Hardware-in-loop bench tests

- serial GNSS replay into ESP32
- SD load while UI updates
- deliberately disconnected GNSS
- deliberately removed/full SD card
- rapid power cycling
- IMU unavailable

### Outdoor tests

- open-sky GNSS rate and accuracy
- antenna placement comparison
- walking start-line crossing
- vehicle low-speed crossing on private/controlled ground

### Track tests

- compare against a commercial reference timer/logger
- validate no missed/duplicate laps
- inspect GNSS quality through high-G corners and under structures/trees if present
- repeat at multiple circuits

## 2. GNSS acceptance tests

### Rate

For a 30-minute log:

- expected number of fixes within tolerance for configured rate
- no unexplained parser gaps
- sequence/timestamp cadence analysed
- queue high-water mark captured

### Quality

Record distributions for:

- horizontal accuracy estimate
- number of satellites
- fix type
- speed accuracy
- heading accuracy

Do not define a universal timing-quality threshold until real track traces are inspected.

## 3. Crossing geometry tests

Required synthetic cases:

1. clean perpendicular crossing
2. crossing exactly at line endpoint
3. movement parallel to line
4. segment ends before line
5. reverse-direction crossing
6. jitter back and forth around line
7. very slow movement
8. large timestamp gap
9. invalid GNSS fix at one side of crossing
10. consecutive valid crossings inside minimum lap time
11. correct crossing after leaving rearm corridor

## 4. Performance tests

Measure:

- GNSS parser CPU usage
- timing engine execution time
- maximum queue depth
- SD write latency percentiles
- display render time
- free heap/PSRAM over a long session

Run at maximum screen activity while GNSS is 25 Hz and SD logging is enabled.

## 5. Soak test

Minimum pre-track soak:

- 4 hours powered
- GNSS active at target rate
- SD logging
- UI cycling or running a simulated session
- IMU active

Pass criteria:

- no reset
- no memory trend indicating a leak
- no GNSS queue overflow
- log can be parsed completely except an intentionally truncated final row if power is removed

## 6. Track validation metrics

For each reference comparison, record:

- device lap time
- reference lap time
- difference
- GNSS quality near crossing
- vehicle speed at crossing
- antenna position
- track/start-line definition revision

Do not cherry-pick the best laps. Keep the complete session comparison.
