# Changelog

All notable changes to this project will be documented here.

The project follows Semantic Versioning once the first firmware release is tagged.

## Unreleased

### Added

- a **Navigation** mode: road speed low and large, the receiver's clock top left, and an
  arrow top right pointing where the car is going with north at twelve o'clock. Three
  readouts the device already computed in three separate modes, on one screen
- the speed is **261 px**, larger than Speed Only's 248 and the largest numeral on the
  device. 248 is the ceiling for a centred block with 30 px margins; at this height the
  number is the only thing across the panel, so it takes a 16 px margin instead and three
  cells reach 564 px. The unit sits under it, as Speed Only's does and for Speed Only's
  reason - beside it would reserve width the three-digit case needs, which is exactly the
  compromise that costs the face its size
- the number and its unit are **centred in the band under the arrow's ring** rather than
  pushed onto the bottom margin, which puts the digits 96 px higher than a first pass that
  sat them low
- the arrow is **absent whenever the heading is not trustworthy**, with the reason in the
  ring where it would have been. The board has no magnetometer, so the direction is course
  over ground and a parked car has no heading at all - not a stale one, not north. The
  gates are Compass mode's, reused rather than restated: the receiver's own `headAcc`
  under 15 degrees, with a half-metre-per-second backstop under it
- **one cause stated once**: with no receiver all three readouts are absent for the same
  reason, and printing it beside the speed, under the clock and inside the ring would read
  as three faults. Every other combination keeps its three separate statuses, because a
  parked car with a fix and a time and no course really is three different things to say
- the mode is appended to `OperatingMode`, so this is **one byte that already existed
  changing value**: no settings format bump, no migration, and a device updating to this
  firmware keeps its settings

- **Race mode has its own running-session screen**: the lap in progress in `mm:ss.sss`,
  the fastest lap of the session, and the time left in it. Race previously borrowed Track
  Day's countdown, which - as that screen's own header says - has nowhere to put a lap
  time, so a road test produced a session that started correctly on pit exit, timed laps,
  and showed none of them. The times were being computed every tick and formatted by
  `ui::present`, and then nothing drew them
- the Race screen is **separate from Track Day's rather than lap fields added to it**.
  Track Day withholding live lap times is a compliance rule, not a display preference, and
  a screen that has no lap labels at all cannot leak one through a mode check that is
  wrong. It consumes the `DeviceViewModel` the presenter already produces, so the
  formatting - including `--:--.---` for a lap nothing has measured yet - is the one the
  host tests cover rather than a fourth copy of it
- nine fixed cells for the lap in progress, so a lap past ten minutes still reads rather
  than losing its leading digit, and each cell is rewritten only when its character
  changes: the current lap advances on every fix and the other two almost never, while a
  repaint is the most expensive thing the device does (#213)

- a **0-X** mode: the time from a standing start to a target speed, counting up from
  `0.00`, stopping the instant the target is reached and turning green. Coming back to rest
  re-arms it; a tap resets it sooner. The target is chosen in `SETUP` -> `0-X TARGET`
- both ends of a 0-X run are **interpolated rather than quantised**. A stationary receiver
  reports a few centimetres per second of noise, not zero, so the start is estimated by
  extrapolating back along the acceleration between the first two moving fixes to where
  speed would have been zero; the finish is interpolated between the fixes straddling the
  target. At 25 Hz a fix lands every 40 ms, so without both the second decimal would be
  decoration. A 0-60 at 4 m/s2 measures 6.71 s against a true 6.7056
- a 0-X run whose fixes dry up is **abandoned, not finished**. The number stops climbing
  either way, which is the whole problem: a stalled run looks exactly like an arrival, so
  it says `LOST FIX` and is never green
- the settings record is at **version 8**, carrying the 0-X target. Unlike appending a
  mode, a new field changes a fixed-width record's length, so this needed a migration
  rather than only a wider validation bound. A v7 device keeps every setting it had and
  takes the default target, because it never had an opinion about one

- a **Compass** mode: a fixed card with north up, a marker travelling its rim, and the
  bearing in degrees at the centre. The heading was already decoded - `headMot` and
  `headAcc` have been reaching `GnssFix` every epoch - so this needed no parser work at all
- Compass refuses to point anywhere when it cannot know. What the receiver reports is
  course over ground, and the board's QMI8658 is six-axis with no magnetometer, so **a
  parked car has no heading at all**: not a stale one, not north, none. The marker is
  hidden and the degrees read `---` whenever `headAcc` is too wide or the car is below
  walking pace, under its own "NO HEADING" state - which is not "NO FIX", because the fix
  is usually perfect and sending somebody to the wrong problem is worse than saying nothing
- the compass card is fixed and only the marker moves. This device draws graphics from
  positioned styled objects rather than canvas paths, so a rotating card would mean
  repositioning every tick on every update - at 20 Hz, the per-frame work that has starved
  the UI task twice. The marker is also only moved when the bearing changes by a whole
  degree, which is not smoothing: the value shown is still exactly what the receiver said

- a **Speed Only** mode: road speed in mph at 248 px, which is the arithmetic ceiling for
  three digits across this panel and two thirds of its height. Reaching it meant centring
  the block on the digits actually shown rather than right-aligning into three fixed cells
  - with the cells pinned, only a two-digit reading can be centred and both `0` and `140`
  sit 67 px off to one side, `0` being the reading on the panel whenever the car is
  standing still. The cells stay fixed width, so no digit shuffles against its neighbour;
  only the block moves, and only at 9 to 10 and 99 to 100
- Speed Only reuses `gps_only_state()` and `kMetresPerSecondToMph` rather than restating
  them, so there is one definition on the device of what "no fix" means and one conversion
  out of metres per second. A stationary car with a good fix reads `0`; an absent or
  unfixed receiver reads `---` and says which
- the GNSS fix queue has a consumer. Accepted fixes are handed to the timing task, lap
  events and gate records are drained from it, and lap index, current, previous and best
  lap reach `domain::UiSnapshot` - which the router previously built with only
  `session_remaining_ms` set. The timing runtime, its engine, its lap state machine and its
  three bounded queues had all been built and host-tested, and nothing in the tree called
  any of them: every fix the device ever produced was read, validated, counted and dropped
  on the floor
- the circuit can be changed while the timing task is running. `start_embedded_timing_runtime`
  refuses a second call, so until now the first circuit selected would have been the only
  one the device could ever time. The swap takes the engine under a mutex and discards every
  queued fix, lap event and gate record with the old geometry, counting the abandoned fixes
  separately from queue drops - one is a deliberate loss, the other is a consumer that could
  not keep up
- `timing::lap_display_times` converts a lap snapshot into the milliseconds a display works
  in. Separated and host-tested because it is where "no lap yet" becomes a wrong number:
  every field is nanoseconds and may be `kUnavailableTime`, which is -1, and dividing that
  by a million yields 0 ms - a believable lap time and the same class of mistake as showing
  an absent receiver as 0 mph

- a **Clock** mode, selectable from the mode carousel, showing a 24-hour GMT clock from the
  receiver's own UTC time. The hour and minute fill the panel at 170 px with the seconds at
  100 px beneath them; the face's own metrics put a single line of `HH:MM:SS` at a 112 px
  ceiling, so splitting the seconds off is what buys the size. GMT is fixed for now and the
  label says so, because for half the year it reads an hour behind every other clock in the
  UK - correct, and guaranteed to look like a fault without the label
- the UTC date and time are decoded from NAV-PVT into `GnssFix::utc`. They were in every
  message and the decoder read straight past them, so nothing reaching the UI was a wall
  clock: `measurement_time_ns` is GPS time of week, which has no date, rolls over weekly and
  runs ahead of UTC by the leap-second count. The receiver already applies that correction,
  so the firmware reads its answer rather than reconstructing one. `nano` - the field #193
  was about - is finally used for what it is: the fraction of the UTC second, and the reason
  a clock shows the second it is actually in rather than the one it is rounding towards
- Clock mode free-runs and says so when the receiver goes quiet, rather than freezing. A
  frozen clock looks completely correct and is wrong by however long it has been frozen,
  with nothing on screen to contradict it; a held one shows how long it has been holding and
  stops claiming a time after an hour, at which point "this is GNSS time" is no longer true

- the receiver's stored configuration is read back from its flash layer with
  `UBX-CFG-VALGET` and reported at boot. This is what makes "survives a power cycle" an
  observation rather than an inference: an acknowledgement says a write was accepted, a
  read-back says the value is actually in the layer that outlives power. It also needs no
  power cycle to establish, which matters because the log from a power-on boot is the one
  this hardware cannot capture - the USB device is inside the chip, so cutting power destroys
  the port a listener would be holding. Confirmed on hardware: nav-pvt on, nmea off, 50 ms
- the firmware reports what the receiver arrived as, before writing anything to it. This is
  the only moment the question can be asked: configuration runs on every boot, so a receiver
  whose stored settings survived and one that reverted to its factory default behave
  identically from the first fix onwards, and "written to flash and acknowledged" cannot
  become "observed to persist" without looking first. It also matters beyond verification -
  a receiver that loses power independently of the board comes back in its default, and
  nothing can handle a reconnect it cannot detect. Three answers, including "said nothing",
  which is not an answer and is reported as such rather than guessed at

- the firmware configures the receiver at boot and confirms it agreed. A NEO-M9N in its
  factory default emits NMEA, which this firmware does not parse, so the device read a
  healthy receiver and understood none of it. `UBX-CFG-VALSET` now enables UBX NAV-PVT on
  I2C, silences NMEA and sets a 20 Hz measurement rate, written to RAM, battery-backed RAM
  and flash so it survives a power cycle rather than working only until the device is
  unplugged
- configuration is acknowledged rather than assumed: the receiver's `UBX-ACK-ACK` or
  `ACK-NAK` is matched against the frame that was sent, and a refusal, a silence, a bus that
  would not carry the frame and a transport that cannot write at all are four different
  reported outcomes. A configuration that silently did not take is the failure most likely
  to pass on the bench and reappear in the car
- `GnssTransport` gained a write path, defaulting to "cannot write" so a transport that
  replays a recorded fixture stays legitimately read-only
- the measured fix cadence is logged whenever it changes, and the dominant rejection reason
  whenever fixes are being refused. The rate is derived from the receiver's own measurement
  timestamps, so it reports what arrived rather than what was requested

- the receiver's stage is logged whenever it changes, and its satellite count once a second
  while searching, so bring-up is captured rather than read off the screen. Six named stages
  ordered the way the link actually fails, from an unreadable bus to an accepted fix. The
  line that existed before this said a transport had been attached, which only ever meant
  the I2C driver accepted an address - adding a device to the bus never talks to the chip
- the first bytes the receiver sends are kept and logged as text when bytes arrive but no
  frame parses. That is the one stage the counters cannot diagnose, because neither NMEA nor
  a corrupted bus produces a checksum error: a checksum is only reached after a sync pair,
  and neither has one. On the first run against real hardware it printed `$GNRMC,...`, which
  identified the fault as a receiver in its factory NMEA default rather than anything wired
  wrongly
- `make check` now checks CHANGELOG.md's structure: a mistyped, duplicated, out-of-order or
  empty section fails rather than silently starting a section nobody reads. It found the
  Unreleased section's own headings were out of order on the first run

- `tools/capture_boot_log.py` and `make firmware-capture-boot` capture a cold boot over the
  board's own USB-Serial-JTAG port, which #181 had concluded was impossible. It was not: a
  reset through RTS is a chip reset the port survives, and the earlier failures were the
  chip silently booting the ROM download stub because DTR is wired to GPIO0 and pyserial
  asserts it on open. `idf.py monitor` refuses to run without a TTY, so it cannot be
  scripted
- a test fixture track pack, `data/track-packs/test/manifest.json`, holding Test Circuit: a
  943 m clockwise public-road loop for exercising timing, GNSS reception and lap logging end
  to end. Its gates are 12 m rather than the 20 m a circuit uses, because the carriageway is
  25 ft. It stays provisional until captured on device: a road has no pit lane to identify
  its start/finish straight and no second source for a line somebody chose, so
  `independently_validated` is unreachable and the engine will not arm until #37 captures it
- `tools/build_track_pack.py` builds any pack whose manifest follows the schema, with the
  rules a pack asserts - its id, its national coverage, whether a venue has an official
  source to cite - declared in that manifest rather than compiled into the builder. The
  provenance gate and the coincident-gate check are not declarable and apply to every pack

- a real circuit that can arm the timing engine. Donington Park GP and National are
  promoted to `independently_validated` from two sources agreeing to 3.8 m. Until now
  every circuit in the UK pack was provisional, so a driver could select any of the 24
  and the engine would never arm at a real venue

- a documented promotion process for track geometry, enforced by the pack builder rather
  than left to review: two sources from distinct origins agreeing within 10 m, or a
  physical capture. A promotion that does not meet the rules stops the build. Restores
  the `### Added` heading, which a previous edit removed, leaving every addition filed
  under Fixed

- I2C transport for the receiver, and the GNSS chain linked into the firmware for the
  first time. The u-blox DDC protocol reads the waiting byte count from `0xFD`/`0xFE`
  before streaming from `0xFF`, rather than reading blindly and discarding padding on a bus
  shared with touch, the IMU and the RTC; an implausible count is refused rather than
  believed, since a NACKed read returns 0xFFFF. GPS Only, the ready dashboard and the
  Diagnostics GNSS page are all driven from the receiver's real state

- GNSS pipeline: bytes in, judged fixes out, and an account of everything lost on the way.
  Work per poll is bounded so the pipeline cannot starve the task it shares a core with,
  the fix queue is bounded so a slow consumer becomes a counted drop rather than a memory
  fault later, and loss is counted by cause rather than as one total. Gaps are measured
  from the receiver's own time of week, never from arrival cadence, so a late poll is not
  mistaken for loss and a lost epoch cannot hide behind a prompt one

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

### Changed

- the **GPS session trigger starts on the start line** rather than the pit exit, which is
  what keeps it when the pit gates go. The start line is crossed every lap, where pit exit
  on a road loop was a line somebody picked; and on a circuit whose start and finish are
  one line, the session and lap 1 begin on the same crossing. `pit_exit_auto_start_enabled`
  is renamed `gps_start_line_enabled` - the stored byte and its meaning are unchanged, so
  the settings record needs **no version bump and no migration**. The byte that held
  pit-entry auto-stop is kept as reserved and written as zero, because reclaiming it would
  change a fixed-width record's length for a setting nothing uses
- a start-line crossing starts a session **only while armed**. Without that, driving out of
  the paddock would begin one: the pit-exit gate was obscure enough that a crossing implied
  intent, and the start line is not. `GateAutomationResult::ignored_unarmed` reports it,
  because the remedy differs from every other refusal
- `SETUP` names the rest period **`REST`**, not `PIT REST`: it is time between sessions,
  spent wherever the driver likes

- the G radar's trace is back to **1.2 seconds**, reverting an increase to three that was
  merged on a bad measurement and then measured properly. Both figures below are with the
  device untouched, which is when the meter reaches `ready` and the trace is actually
  drawn:

  | | 24 points | 60 points |
  |---|---|---|
  | object work, mean | 4,889-6,252 us | 13,658-18,303 us |
  | share of the 50 ms display period | 12-16% | 36-41% |
  | worst repaint | 38,532-41,095 us | 53,101-55,442 us |
  | repaints per 10 s | 147-152 | 166-170 |

  The first measurement was taken while the device was being moved by hand, on the
  reasoning that a moving dot dirties more screen area than a parked one. Motion keeps the
  gravity calibration from completing, so `resolve()` stays invalid, the meter state is not
  `ready`, and `update()` takes the branch that hides every dot without positioning or
  restyling it. The clean 1,727 to 2,395 us scaling that appeared to make three seconds
  nearly free was the cost of drawing nothing

- the repaint figures above are a pre-existing problem this work measured rather than
  caused: **40 ms against a 25 ms service period at the current 24 points**, on the task
  that also dispatches the receiver poll. At 25 Hz a fix is due every 40 ms. Tracked
  separately; the trace cannot grow until it is addressed

- the receiver now runs at **25 Hz**. Measured on hardware: 24.9 Hz observed, 57 frames,
  57 accepted, nothing rejected, no checksum errors, and 40 ms read back from the
  receiver's flash layer. The part was never in doubt - it was separately measured at
  25.3 frames/s - and 20 Hz was proven first, which is what #16 and ADR-005 asked for
- the GNSS poll runs at 25 ms rather than 50. It was rate-limited to exactly 20 Hz, so at
  a 40 ms fix rate it would have been **slower than the receiver it reads** and fixes would
  have queued in the receiver's buffer permanently. Raising the receiver's rate without
  this would have made things worse, not better
- the service tick now drives two cadences instead of one. The GNSS poll runs every tick;
  the IMU, the session lifecycle and the screens keep their 50 ms period, because the
  receiver's deadline is not the display's and redrawing live views at 40 Hz costs the task
  that has starved twice
- this also restores the gap detector's intended margin. The pipeline has always expected
  25 Hz, so while the receiver ran at 20 its 50 ms intervals sat 10 ms under a 60 ms gap
  threshold rather than the 20 ms the 1.5x multiple was chosen to give. All three now agree

### Removed

- **pit lane entry and exit, entirely**: the two timing gates, the settings behind them,
  the session automation's stop path, the capture steps, the gate fields in a track
  definition, and their documentation. Too much complexity for what they earned
- `TimingGate` is `start` and `finish`, and `kTimingGateCount` is 2. A track definition's
  `CircuitGateDefinitions` loses both pit gates, which takes the **track schema to
  version 4**. A schema-3 file still loads - its pit gates are parsed and discarded -
  because the 26 circuits already written to cards are otherwise correct and rejecting
  them would cost a device its whole track list for gates nothing reads
- gate capture walks **two gates rather than four**
- `SessionController::stop_from_pit_entry` and `CompletionReason::pit_entry`. The log
  format's `SessionCompletionReason::pit_entry` becomes `reserved_pit_entry`: it is kept,
  not produced, so sessions already recorded on cards still decode to a reason the reader
  knows
- `pit_entry_auto_stop_enabled`. Stopping has no start-line equivalent and gains none -
  stopping on the start line would end a session after one lap - so stopping stays the
  driver's

### Fixed

- gate capture **stopped asking for eight endpoints that no longer exist.** Removing pit
  entry and exit (#227) took the circuit from four gates to two, and the controller
  followed - but the text it hands the screen did not, so the device went on instructing
  the driver to capture 8 endpoints across FOUR GATES while save unlocked at four across
  two, and the selection line counted up to `8` it could never reach. The count is now
  taken from `kGateCaptureEndpointCount`, or the gates are named instead of counted
  (`CAPTURE BOTH ENDS OF THE START AND FINISH LINES`), and a host test asserts the
  instructions against the stated shape rather than a number typed into it (#204)
- a capture refused by validation now **says which check refused it.** `SAVE BLOCKED - GATE
  GEOMETRY FAILED SAFETY VALIDATION` named none of the three things that could have gone
  wrong, to somebody standing at a circuit with the engine off. A line whose ends are under
  a metre apart, a line outside the circuit's geofence and a line too long or too close to
  the direction of travel have nothing in common but the refusal, and three different
  remedies. `serialize_track_definition` already round-trips its candidate through
  `load_track_definition` - that is what keeps invalid geometry off the card - so the
  report it was discarding is now passed back and both the panel and the log name it. Where
  one result covers several checks, all of them are listed rather than one being picked and
  presented as the cause (#204)

- the rule sending a running session to **Race's screen rather than Track Day's is now a
  host-tested function** instead of a comparison inside the router's screen lookup. Track
  Day withholding live lap times is a compliance rule, and the screen it is drawn on has
  no lap labels anywhere in it - so that mode check was the only thing enforcing the rule,
  and nothing asserted it. `ui::running_screen_for` states it once, the default is the
  screen that *cannot* show a lap time, and the tests pin both halves for every mode plus
  the invariant tying them together: a mode is drawn on the Race screen exactly when it is
  permitted lap times (#219)
- **no mode claims a live lap delta any more.** Race was granted `lap_delta` back when its
  running screen had no lap fields at all; the screen it has now shows three figures and
  no delta, and the reference mapping a trustworthy one needs is still #58. A permission
  for something nothing draws describes a display that does not exist, which is the same
  mistake as a plausible reading - so the flag is false everywhere until a screen draws
  one, and a test holds it there (#219)
- the Race screen's **track name is written only when it changes.** The router handed it
  over on every display tick while the one thing it names - the armed track - cannot
  change during a session, and `lv_label_set_text` invalidates whether or not the text
  differs. That was a repaint of the widest label on the screen 25 times a second, next to
  three lap rows that were already change-gated for exactly this reason (#213)

- `SettingsField` gained `acceleration_target` without `kSettingsFieldCount` or the name
  array following it, so naming that field read past the end of the array. Found while
  removing the two pit entries from the same list

- the ready dashboard **reports the receiver as it is now, not as it was at boot**.
  `ready_snapshot` read the health live and correctly, but `refresh_ready` was only
  reached from boot, a settings save and a track change, and the receiver acquires its
  first fix after the boot sequence has already drawn the screen. With 16 satellites and a
  24.9 Hz fix stream the dashboard still showed what was true at 2.8 seconds. It now
  refreshes from the display cadence, gated on the dashboard being the active screen and
  on a fingerprint of the drawn state having changed - an idle dashboard costs a struct
  comparison, because a repaint costs about 40 ms against a 25 ms service period (#213)
- Race mode no longer claims **`lap times pending GNSS`** whatever the receiver is doing.
  Honest while nothing consumed fixes; since the timing runtime was wired it was both
  wrong and the most alarming thing the screen could say while everything worked. The note
  now distinguishes no circuit armed, waiting for a fix, and lap times live
- `logging_available` and `session_active` on the dashboard are read rather than
  hardcoded to `false`

- the **GPS session trigger now starts a session**. Choosing it armed the device and showed
  `WAITING FOR THE LINE`, and nothing could ever start the session: only a manual START and
  IMU launch detection reached `begin_session`. The mechanism existed - `gate_automation`
  starts on pit exit and stops on pit entry, host-tested, with `session_trigger.cpp` already
  setting `pit_exit_auto_start_enabled` from the trigger - and the crossings were already
  being read off the timing queue and discarded in an empty loop. The two halves had never
  been introduced
- a session started by a gate crossing now runs for **the configured duration**.
  `SessionController::start` takes its duration from the configuration already saved, and
  that was only ever written inside the manual start path, so a crossing-started session
  would have used `SessionConfiguration`'s own twenty-minute default - plausible, and wrong
  for anyone who set something else. The configuration now follows the settings: written at
  boot and whenever a duration is edited
- a session ended by a pit entry is **recorded as a pit entry**. The summary's completion
  reason was hardcoded to `driver_stop`, which was harmless while a driver stop was the only
  way a session could end

- recorded G peaks were bounded by what the radar can draw rather than by what the car did.
  The display clamps a reading to 2 g so the dot stays on the dial, and that clamp was
  applied before the peaks were taken - so a session summary recorded the limit as though
  it were a measurement. A hand-shaken session filed `2577 milli-g`, which is two axes
  pinned at the clamp rather than anything that happened.
  It bites unevenly, which is why it survived: a car on road tyres reaches about 1.5 g
  sideways and never approaches the limit, but a kerb strike passes 2 g vertically without
  trying, and was being filed as exactly 2.00 with nothing to say it had been truncated.
  The measurement now feeds the peaks and the display clamps its own copy. A credibility
  bound remains at 10 g, because past that is a sensor fault rather than a lap and the
  summary validator refuses the record anyway - but a peak that reaches it is now flagged
  as truncated, since a bound nobody is told about is the defect itself rather than the fix

- the carousel stopped accepting taps, which made every menu, mode, track and setting
  unselectable. `attach_gesture_input` keeps its bindings in a fixed table and refuses
  silently once it is full; Speed Only added a ninth screen to a table of eight, and the
  ninth was the carousel.
  The symptom hid its own cause. The menu still opened, because the hold that opens it
  lands on the dashboard's binding. The carousel still moved between entries, because its
  chevrons are real buttons and owe nothing to the gesture layer. It drew correctly. It
  simply ignored every press, on every entry - which reads as a problem with whichever
  entry you happened to try.
  The table now holds sixteen, but the cap was only what tripped this. The defect is that a
  screen could ask for input, be refused, and go on looking entirely normal while ignoring
  the driver for the life of the device. Refusals are counted and reported at boot as an
  error, and every shell gesture is now logged with the outcome it produced, so a press
  that goes nowhere leaves a trace instead of nothing at all

- `decode_nav_pvt` added UBX-NAV-PVT's `nano` field to iTOW when building
  `measurement_time_ns`. It reads like a sub-millisecond correction and is not one: it is
  the fractional part of the UTC second in the same message, ranging a full second either
  way. Added to a time of week that already carries its own sub-second part, it moved the
  timestamp by up to a second in either direction.
  On hardware this made measurement time run *backwards* - observed jumping 760 ms into the
  past on checksum-valid frames - which the quality gate correctly refused as non-monotonic
  at roughly every other epoch, and which also halved the cadence the pipeline derived from
  those same timestamps. The receiver had been emitting exactly its configured rate
  throughout. With the term removed: 49 frames, 49 accepted, 0 rejected, 19.9 Hz measured
  against 20 Hz configured.
  Every lap time this would have produced was going to be computed from a clock that moved
  backwards twice a second. It was invisible to the host tests because a zero-initialised
  NAV-PVT payload has `nano` of zero - and worse, `test_ubx_parser.cpp` asserted the stray
  term was present, so the test agreed with the bug rather than catching it. Both fixtures
  now carry a large negative value, which is the sign that breaks monotonicity

- GPS Only and the Diagnostics GNSS page took the satellite count and fix type from the last
  *accepted* fix, so neither could climb during acquisition: a cold start reports a rising
  satellite count and no position, every fix is refused by the quality gate, and the screens
  showed a flat `NO FIX / 0`. A receiver that was working and searching looked exactly like
  one that was not plugged in - the distinction GPS Only exists to draw. Both now read the
  last fix the receiver *sent* for those two fields, while position, speed and accuracy stay
  behind the gate, so nothing refused is ever shown as usable

- `CONTRIBUTING.md` told contributors to install the pinned requirements into whatever
  `python` resolved to, which is exactly the drift that was silently disabling date
  validation. It now says `make venv`, matching `docs/DEVELOPMENT_SETUP.md`
- the README claimed the firmware "only proves that the ESP32-S3 toolchain and board
  connection work" with "display, touch and GNSS support" as later milestones, contradicting
  its own status section 190 lines above, and said all public-map geometry was timer-only
  after two definitions had been validated

- the boot log contradicted itself 270 ms apart: `app_main` ended startup with an
  unconditional `no GNSS transport yet`, which #178 made false when it wired the transport,
  while the screen router had already reported `gnss: transport attached over i2c`. Anyone
  diagnosing GNSS would have believed the later line
- a circuit that could not arm the timing engine read `TIMER ONLY - TRACK DATA` whatever
  the reason, so honest provisional geometry was presented exactly like a corrupt file and
  sent the driver to inspect their card instead of capturing gates. All seven
  `TrackApplyResult` causes collapsed into one message, and the reason was logged - to a
  console unreadable on this board - and then discarded. The ready page now distinguishes
  provisional geometry from a missing or unreadable definition, and the selected track
  carries its own state and colour on its own line so it is legible with no GNSS fix, which
  is when the whole track-state message was previously unreachable
- the pack archive was named `uk-track-pack-v1.zip` whatever it contained, so building a
  second pack into the same directory silently overwrote the first one's archive; it is now
  named from the pack id
- `make check` ran against whatever `python3` resolved to, so a system interpreter missing
  `rfc3339-validator` produced one confusing failure in the schema test while
  `tools/validate_tracks.py` and `tools/track_workbench.py` silently stopped validating
  `format: date-time` - meaning `provenance.verified_utc`, the field the track provenance
  gate reads, would accept any string. The Makefile now prefers `.venv/bin/python` when it
  exists, `make venv` creates it from the pinned requirements, and
  `tools/check_python_env.py` refuses an interpreter that cannot validate what the suite
  asserts

- five start/finish gates in the UK pack were on the wrong feature. Croft's was on a way
  named *Rallycross* and Silverstone's international profile on *Stowe Circuit*, both
  tarmac the circuit's cars never drive, so neither gate could ever have triggered.
  Silverstone's national profile, Pembrey and Llandow had start lines whose coordinates
  were copied from their pit entry, placing the line at the junction where the preceding
  corner meets the pit straight. All five are now identified from pit-lane adjacency, and
  the pack build refuses a start gate that shares a pit gate's position

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

- Donington Park's start/finish gate was on the wrong part of the circuit. It sat on
  OpenStreetMap way 841515325, *Melbourne Loop (up)*, 82 m from the Wheatcroft Straight
  that the pit lane opens onto, with its heading 162 degrees out - pointing back down the
  circuit. A car crossing the real line would never have triggered it, and the National
  layout does not drive the Melbourne Loop at all. Found by checking every cited
  OpenStreetMap way against what it is actually named
