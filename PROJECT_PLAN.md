# Project Plan

## 1. Purpose

Build a reliable standalone track session timer and GNSS lap timer around a larger AMOLED display and a 20 Hz minimum GNSS receiver.

The project should reach useful hardware as early as possible. The plan therefore separates hardware bring-up, GNSS capture, host-side timing validation and final UI integration rather than attempting to build the complete product in one pass.

## 2. Definition of done for first usable release

A first usable release is achieved when one enclosed prototype can:

- boot reliably from vehicle USB power or its supported battery arrangement
- show the session timer clearly in daylight
- configure and acquire the NEO-M9N at 20 Hz minimum
- log GNSS data to microSD without dropping fixes during a normal session
- load a track definition from local storage
- detect start/finish crossings in the correct direction
- calculate and display current, previous and best lap
- continue the normal session countdown if GNSS is lost
- survive at least one complete track day without software reset or data corruption
- export enough logged data to reproduce every detected lap on a host computer

## 3. Delivery strategy

Development is split into gated milestones. Do not start a later feature because it is interesting if an earlier hardware or timing gate is unresolved.

### M0 - Repository and toolchain bootstrap

**Goal:** prove a repeatable development environment.

Deliverables:

- ESP-IDF environment documented
- minimal ESP32-S3 firmware builds
- device flashes and serial monitor works
- repository conventions agreed
- first GitHub issues created

Exit gate:

- clean clone to successful firmware build on a second shell/workstation

### M1 - Display board bring-up

**Goal:** prove every onboard peripheral that the product depends on.

Deliverables:

- RM690B0 display initialization
- 600 x 450 framebuffer strategy confirmed
- brightness control
- FT6336 touch input
- QMI8658 IMU input
- microSD mount/read/write
- RTC access
- power/battery status access if exposed reliably
- measured display refresh and CPU/PSRAM load

Exit gate:

- hardware diagnostic screen can exercise display, touch, IMU and storage for 30 minutes without reset

### M2 - GNSS electrical and protocol bring-up

**Goal:** receive clean 20/25 Hz GNSS data without disturbing the UI.

Deliverables:

- dedicated UART wiring documented
- NEO-M9N configured using UBX
- update rate set to 20 Hz initially, 25 Hz evaluated
- only required UBX messages enabled
- fix quality, horizontal accuracy, satellite count, speed and heading captured
- optional PPS/timepulse input captured for latency/clock diagnostics
- raw or minimally processed GNSS log to SD

Exit gate:

- 30-minute outdoor log with expected update rate and no unexplained serial loss

### M3 - Host-side lap timing engine

**Goal:** validate the mathematics independently of embedded UI timing.

Deliverables:

- local coordinate conversion
- segment/start-line intersection
- direction validation
- crossing interpolation using GNSS timestamps
- hysteresis and duplicate-crossing suppression
- minimum-lap-time guard
- synthetic tests
- recorded-trace replay tool

Exit gate:

- deterministic host tests pass and manually inspected traces produce expected crossing times

### M4 - Embedded lap timing MVP

**Goal:** calculate laps live on the ESP32-S3.

Deliverables:

- GNSS ingestion task/queue
- timing engine component
- current lap start timestamp
- previous lap
- best lap
- GPS state and quality indicator
- track start line loaded from SD

Exit gate:

- repeated walking/driving test crossings create exactly one lap per valid crossing

### M5 - Port session timer behaviour

**Goal:** recover the proven TrackSessionTimer product behaviour.

Port or reimplement:

- ready/configure/run/rest workflow
- configurable session duration
- green/yellow/amber/red progress semantics
- overtime state
- safe stop/abort gesture behaviour
- brightness and orientation behaviour
- post-session review

Exit gate:

- session countdown operates correctly with GNSS present, absent and deliberately failing

### M6 - Driver UI and lap presentation

**Goal:** make the device glance-readable at speed.

Deliverables:

- large current lap display
- previous and best lap
- lap number
- GNSS quality state
- session time remaining
- deliberate colour hierarchy
- no small essential text
- touch actions locked or constrained during active driving

Exit gate:

- static cockpit readability test in full daylight and low-light conditions

### M7 - Track database and start-line capture

**Goal:** remove source-code changes from normal track setup.

Deliverables:

- versioned local track JSON schema
- track selection
- nearest-track suggestion using a broad geofence
- start/finish line endpoints and crossing direction
- optional sectors in schema even if sectors are not yet displayed
- safe user capture workflow for an unknown circuit

Exit gate:

- a new track can be added or captured without rebuilding firmware

### M8 - Logging and replay

**Goal:** make every timing decision auditable.

Deliverables:

- session metadata file
- GNSS trace file
- lap-event file or event rows
- dropped-sample counters
- firmware/hardware version in every session
- host replay producing the same lap events from logged fixes

Exit gate:

- embedded and replay lap times agree within the defined numeric tolerance

### M9 - Enclosure, power and vehicle integration

**Goal:** produce a robust single-unit prototype.

Deliverables:

- rear GNSS/power pod or revised case
- strain relief
- secure antenna connector
- protected wiring
- mount interface
- thermal check
- vibration check
- power-interruption behaviour

Exit gate:

- device survives bench vibration/handling and repeated power cycles without loose connections or corrupt storage

### M10 - Track validation and release candidate

**Goal:** prove the system in the real environment.

Test progression:

1. walking crossing tests
2. road/private-land low-speed tests where legal and safe
3. passenger/static logging comparison
4. track session with a commercial lap timer as a reference
5. repeated sessions at different circuits

Exit gate:

- no duplicate/missed crossings in accepted test sessions
- differences against the chosen reference are understood and documented
- all critical defects closed

## 4. Priority order

1. Reliability
2. Correct lap event detection
3. Readability
4. Reproducibility of timing from logs
5. Easy track setup
6. Additional metrics and visual polish

Live delta, sectors, Wi-Fi transfer and richer analytics must not displace the first five priorities.

## 5. Project risks

| Risk | Impact | Mitigation |
|---|---|---|
| AMOLED difficult to read in direct sun | Driver cannot use device | Physical daylight test before UI investment; high-contrast theme; consider shade/visor |
| Waveshare display driver integration harder than expected | Delays UI | Bring up display first; isolate BSP from application code |
| GNSS serial traffic competes with UI | Dropped fixes | Dedicated FreeRTOS task, queue and bounded logging |
| Position noise causes false start-line events | Bad lap times | Direction, hysteresis, quality thresholds, minimum lap time and trace replay |
| SD write latency blocks timing | Missed GNSS data | Buffered writer task and bounded queues |
| GNSS antenna placement poor | Inconsistent lap timing | External antenna, clear sky view, quality metrics and installation guidance |
| Power interruption corrupts files | Lost session data | Append-safe format, periodic flush strategy, close on controlled shutdown where possible |
| Porting old code imports old hardware assumptions | Architecture debt | Port behaviour, not pin-level implementation |
| Scope expands into telemetry/CAN | Release slips | Treat CAN as a separate future project/optional interface |

## 6. Release naming

Suggested progression:

- `v0.1-hw` display board bring-up
- `v0.2-gnss` high-rate GNSS logging
- `v0.3-timing` embedded lap detection
- `v0.4-session` original session workflow restored
- `v0.5-ui` driver UI
- `v0.6-tracks` track database
- `v0.7-enclosure` integrated prototype
- `v0.9-rc1` track-tested release candidate
- `v1.0.0` first stable track-day release
