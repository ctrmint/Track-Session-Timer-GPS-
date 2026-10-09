# Changelog

All notable changes to this project will be documented here.

The project follows Semantic Versioning once the first firmware release is tagged.

## Unreleased

### Fixed

- G peaks were never scoped to a session, so they described everything since boot; the meter
  was still being told no session was ever running
- the vertical axis was computed and then dropped before it reached the meter
- Review rendered its summary rows at 0.82 mm, which on this panel reads as an empty screen

- the launch sensitivity ladder ran to 4 g, which a car cannot reach as forward
  acceleration, so six of its ten choices could never fire and nothing was offered below
  0.5 g where a deliberate pit exit sits; stored values are snapped onto the new ladder
  rather than failing validation and resetting every other setting

- section menus opened on their first item rather than the one in force, so Mode, Track and
  Trigger could not tell the driver what was set, only let them change it
- the menu carousel was told there were four items while the shell offered five, leaving
  Diagnostics selectable but never drawn

- the start page kept the durations it read at boot, so a session or rest period changed in
  the menu was obeyed by the timer but still shown at its old value until a restart
- durations were written two ways at once, so a one minute session read "1 MIN SESSION"
  beside a "0:12 REST"; every duration is now minutes and seconds

- a session that ran out simply stopped: the rest period was modelled and tested but never
  routed to a screen, and overtime rendered identically to a session still running

- time settings could not reach most of their own range: the average lap picker stopped at
  3:00 against a field holding 59:59, and the durations at 60 minutes against 24 hours
- saving on the roller used LVGL's 400 ms long press, which committed a value while it was
  still being chosen

- the G meter subtracted a gravity reference frozen at calibration, so every later change
  of tilt read as acceleration at sin(angle) - 0.17 g on a 10 degree banked corner - and
  peak-hold latched those artefacts rather than averaging them away; attitude is now
  tracked with the gyroscope
- the vehicle frame was built with "up" pointing down, which inverted the lateral axis and
  swapped the recorded left and right peaks
- rest detection accepted a device that was turning

- LVGL allocated from a fixed 64 KB pool, so large glyph bitmaps evicted and
  re-rasterised on every draw and starved the UI task; it now uses the ESP-IDF heap
  with glyph bitmaps in PSRAM
- live views were refreshed at the LVGL loop rate rather than a sensible one

- the scaled carousel icon drove LVGL's software image transform hard enough to starve
  the UI task, tripping the task watchdog and freezing the menu
- the G meter showed raw accelerometer axes, so the dot sat wherever the unit was tilted
  instead of at the centre
- a swipe also fired a press, because LVGL sends RELEASED before SHORT_CLICKED
- swipes starting on a clickable child never reached the screen's gesture handler

- RM690B0 partial redraws rendered as offset horizontal bands; flush areas are now
  aligned to even columns

### Added

- GNSS fix-quality validation and receiver health: each decoded observation is completed
  with its arrival time and sequence, then judged against a deliberately loose receiver
  gate — invalid status, arrival or measurement order, a repeated epoch, unusable accuracy,
  or motion no car performs. The first failing check wins, so a refusal always has one
  deterministic reason, and a rejected fix is kept whole so a replay can apply a different
  policy to the same evidence. Receiver health separates nothing connected from connected
  and still searching

- GPS Only mode: a fourth selectable Mode showing road speed, position and the receiver's
  own report, for diagnosing the receiver on the bench, in the car and at a venue. With no
  transport yet it reads "NO RECEIVER"; nothing on it renders an unknown value as a zero,
  since a stationary car and a receiver that has never seen a satellite both report zero
  speed

- Waveshare board hardware configuration: 16 MB flash, 8 MB octal PSRAM, 240 MHz CPU
- RM690B0 AMOLED panel bring-up over QSPI with LVGL 9 display registration
- FT6336 touch input registered as an LVGL pointer device
- on-device screen routing for the ready, setup, review and diagnostics screens
- microSD mount, inspection and opt-in format support
- QMI8658 6-axis IMU driver on a shared board I2C bus
- gravity auto-calibration: centres the G meter at any mounting angle and corrects
  sensor scale
- radar-style G meter used as the G-Only display
- vertical G alongside the lateral and longitudinal pair, for kerbs and compressions
- gyroscope zero-rate offset measured at rest and removed, 4.4 dps on this board
- device settings persisted in NVS, so Mode survives a reboot
- bounded UBX parser behind a byte-stream transport seam, with NAV-PVT decoded into the
  existing fix model and offsets verified against the u-blox interface description
- Review presents a session as a carousel, one value per screen, in the same visual
  language as the configuration menus
- a caption line on carousel entries, for when a label alone does not say what it is
- session records kept on the card, so a driver's sessions outlive a power cycle
- a finished session leaves a record: duration, overrun and peak G on every axis, shown in
  Review
- vertical G recorded alongside the horizontal pair, with kerbs and compressions kept apart
- the gated menu opens on REVIEW, which is wanted the moment a session ends
- top-level TRIGGER selection: MANUAL starts on the button, IMU on a launch, GPS at the line
- pending phase on the running screen for a session armed and waiting for its trigger
- overrun timer counting up in deep purple once a session reaches 00:00, with the lap
  estimate replaced by OVER RUN
- rest period shown on the running screen, counting down on the same ramp as a session
- double tap to end a session or its overrun for the rest period, and rest for the
  dashboard
- two-column minutes-and-seconds roller for average lap, session and rest duration, with
  drag, flick momentum, and a deliberate hold to save
- session and rest durations stored as seconds rather than whole minutes
- Track Day running-session screen: countdown, estimated laps as a float, and a
  decaying session bar
- five-band countdown colour ramp blending proportional and absolute thresholds
- large fixed-cell countdown rendered from an embedded font at any size
- top-level track selection that loads the chosen circuit and arms the timing engine
- the selected circuit and its timing readiness are shown on the start page
- top-level Mode selection: Track Day, Race and G-Only
- one-press direct value selection for device settings, replacing increment stepping
- press-and-hold gated menu with swipe carousels for Mode, Setup, Review and Diagnostics
- abstract touch input contract decoupling screens from LVGL and the touch driver
- microSD-backed track catalog: packs are read from the card at boot
- track loader that applies a selected definition to the timing engine
- containerised flash, monitor and device-info make targets
- initial repository architecture
- hardware BOM
- GNSS lap timing design
- ESP-IDF bootstrap application
- host-side geometry reference tests
- initial GitHub issue plan
