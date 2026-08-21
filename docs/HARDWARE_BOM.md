# Hardware Bill of Materials

Baseline checked: **2026-08-12**.

This is a **UK project**. Prototype procurement should therefore use UK-facing suppliers where practical, prices should be recorded in **GBP**, and VAT should be shown explicitly. Overseas direct purchase should be treated as a fallback rather than the baseline procurement route.

Prices and stock below are a point-in-time reference only and must be rechecked before ordering.

## A. UK procurement baseline

| Ref | Qty | Item | Manufacturer part / SKU | Preferred UK supplier | Supplier reference | Baseline UK price | Procurement link | Status / reason |
|---|---:|---|---|---|---|---:|---|---|
| H1 | 1 | Waveshare ESP32-S3 2.41 inch AMOLED Touch Display Dev Board **with case** | `ESP32-S3-Touch-AMOLED-2.41-B` | The Pi Hut | `WAV-30589` | **£51.90 inc VAT** | https://thepihut.com/collections/waveshare/products/esp32-s3-2-41-amoled-touch-display-dev-board-with-case-600x450 | **Selected**. 600 x 450 AMOLED, 800 cd/m2 stated brightness, touch, ESP32-S3R8, 8 MB PSRAM, 16 MB flash, QMI8658 IMU, RTC, microSD/TF and supplied protective case. |
| H2 | 1 | SparkFun GPS Breakout, u-blox NEO-M9N, **SMA** (Qwiic) | `GPS-17285` / manufacturer part `17285` | DigiKey UK | `1568-17285-ND` | **£57.99 ex VAT / £69.59 inc VAT** | https://www.digikey.co.uk/en/products/detail/sparkfun-electronics/GPS-17285/13561758 | **Selected**. Genuine NEO-M9N breakout, 25 Hz max navigation rate, 3.3 V logic, UART/I2C, UBX support and integrated SMA antenna connector. |
| H3 | 1 | Taoglas Magma X active GNSS antenna, magnetic mount, 3 m RG-174, SMA(M) | `AA.170.301111` | RS Components UK | `2857133` | **£22.15 ex VAT / £26.58 inc VAT** | https://uk.rs-online.com/web/p/gps-antennas/2857133 | **Selected**. Covers all four constellations the M9N tracks concurrently, including BeiDou at 1561 MHz, which the common 1575-1610 MHz antennas miss. 1.8-5.5 V suits the breakout's 3.3 V bias; 26-32 dB LNA; IP67; automotive-grade manufacture. |
| H4 | 1 | High-endurance microSD | 8 to 32 GB | UK supplier | TBD | TBD | TBD | Session trace and event logging. FAT32 during development. |
| H5 | 1 | USB-C data/power cable | quality short cable | UK supplier | TBD | TBD | TBD | Programming and bench power. |
| H6 | 1 | 5 V vehicle USB supply | fused, good-quality automotive adaptor | UK supplier | TBD | TBD | TBD | Prototype vehicle power path. Do not connect raw vehicle 12 V to the Waveshare board. |
| H7 | 1 | Rear GNSS enclosure/pod | 3D printed prototype | UK fabrication / in-house | custom | TBD | n/a | Holds the GPS-17285 securely and exposes/protects the SMA antenna connection. |
| H8 | as needed | M2/M3 fasteners, threaded inserts and spacers | stainless/brass | UK supplier | TBD | TBD | TBD | Vibration-resistant assembly. |

**Known selected-electronics subtotal:** **£148.07 inc VAT** for H1 + H2 + H3 at the checked single-unit prices. This excludes microSD, power, enclosure fabrication and delivery.

### Procurement notes

- The Pi Hut listing is the preferred UK purchase route for H1 and identifies the board as Waveshare SKU `WAV-30589`.
- DigiKey UK is the preferred UK-facing purchase route for H2 and lists SparkFun manufacturer part `17285` as DigiKey part `1568-17285-ND`.
- DigiKey showed **313 units in stock** when checked on 2026-08-12. Stock and pricing are volatile and must not be treated as contractual values in the repository.
- DigiKey's UK site displayed the H2 unit price as £57.99 excluding VAT and £69.588 including VAT. The BOM rounds the VAT-inclusive value to £69.59.
- The selected H2 is the **SMA variant**. The previous U.FL breakout (`GPS-15712`) and U.FL-to-SMA pigtail are no longer part of the baseline design.

## B. Display/controller board

### Selected: Waveshare ESP32-S3-Touch-AMOLED-2.41-B

Preferred UK procurement:

- Supplier: **The Pi Hut**
- Pi Hut SKU: `WAV-30589`
- Product page: https://thepihut.com/collections/waveshare/products/esp32-s3-2-41-amoled-touch-display-dev-board-with-case-600x450
- Checked price: **£51.90 inc VAT**

Manufacturer reference:

- Waveshare product: https://www.waveshare.com/esp32-s3-touch-amoled-2.41.htm

Verified characteristics:

- ESP32-S3R8
- 8 MB PSRAM
- 16 MB flash
- 2.41 inch AMOLED
- 600 x 450 resolution
- RM690B0 display driver
- FT6336 capacitive touch
- stated brightness 800 cd/m2
- QMI8658 6-axis IMU
- RTC
- TF/microSD support
- I2C, UART, USB and 34-pin GPIO header
- `-B` variant supplied with case

Why this board:

- materially larger and higher-resolution display than the original 1.28 inch TrackSessionTimer hardware
- strong stated brightness for open-cockpit testing
- retains an onboard IMU for G and launch-related features
- microSD support is already present for session logging
- exposed UART is suitable for the dedicated GNSS connection
- the supplied case gives the prototype a mechanically useful front enclosure immediately

The screen choice remains subject to direct-sunlight cockpit testing before the final enclosure is frozen.

## C. GNSS receiver

### Selected: SparkFun GPS Breakout, NEO-M9N, SMA (Qwiic), GPS-17285

Preferred UK procurement:

- Supplier: **DigiKey UK**
- Manufacturer: SparkFun Electronics
- Manufacturer part: `17285` / SparkFun SKU `GPS-17285`
- DigiKey part: `1568-17285-ND`
- Product page: https://www.digikey.co.uk/en/products/detail/sparkfun-electronics/GPS-17285/13561758
- Checked price: **£57.99 ex VAT / £69.59 inc VAT**

Manufacturer product reference:

- SparkFun: https://www.sparkfun.com/sparkfun-gps-breakout-neo-m9n-sma-qwiic.html

Key characteristics relevant to this project:

- genuine u-blox NEO-M9N receiver
- **25 Hz maximum navigation update rate** with four concurrent GNSS constellations
- GPS, GLONASS, Galileo and BeiDou support
- integrated **SMA connector** for the external antenna
- 3.3 V VCC and I/O
- UART and I2C interfaces
- NMEA, UBX and RTCM protocol support
- receiver timepulse capability
- onboard backup battery for warm/hot-start assistance

### The 25 Hz figure, verified against the data sheet

Checked 2026-08-21 rather than taken on trust, because every alternative examined turned out
to trade update rate against the number of constellations, and it would have been careless to
apply that scrutiny only to the alternatives.

`NEO-M9N-00B_DataSheet_UBX-19014285` gives the maximum navigation update rate per constellation
configuration:

| GNSS configuration | Max PVT update rate |
|---|---:|
| **GPS + GLONASS + Galileo + BeiDou** | **25 Hz** |
| GPS + GLONASS + Galileo | 25 Hz |
| GPS + GLONASS | 25 Hz |
| GPS + BeiDou | 25 Hz |
| GPS + Galileo | 25 Hz |

The rate does not fall as constellations are added. **25 Hz is available with all four
concurrently**, which is what this project needs and what makes the M9N unusually well suited
to it.

### Why the newer u-blox parts are a downgrade here

This is the substitution most likely to be proposed again, so it is recorded with figures.

| Part | Four constellations | Best case |
|---|---:|---|
| **NEO-M9N** (M9) | **25 Hz** | 25 Hz |
| MAX-M10S (M10) | 10 Hz | 20 Hz, and only with three constellations and a raised CPU clock |
| ZED-F9P (F9) | ~9 Hz indicated | 25 Hz, and the table indicates that is a reduced configuration |

The M10 platform is newer and worse for this application. It is optimised for low power, and
its default clock rate supports only 10 Hz with four constellations; higher rates require
reconfiguring the clock and dropping a constellation. This device runs from vehicle USB, so
the power saving buys nothing and the rate ceiling costs everything.

The firmware acceptance requirement remains **20 Hz minimum**. Development should establish reliable 20 Hz operation first, then evaluate 25 Hz with the display, SD logging and all normal tasks running concurrently.

## D. GNSS antenna

### Selected: Taoglas Magma X `AA.170.301111`, RS Components `2857133`, £26.58 inc VAT

Chosen 2026-08-21, filling the last unspecified item in the reception chain.

### Band coverage is what decides this, and it is easy to get wrong

The M9N tracks four constellations concurrently, and they do not share a frequency:

| Constellation | Frequency |
|---|---:|
| GPS and Galileo | 1575.42 MHz |
| **BeiDou B1** | **1561.098 MHz** |
| GLONASS L1 | 1602 +/- 8 MHz |

Many antennas sold as "GPS/GNSS" are tuned **1575-1610 MHz**, which covers GPS, Galileo and
GLONASS but sits **above BeiDou**. SparkFun's own `GPS-14986` at around GBP 18 is one of them.
Fitting one costs a quarter of the receiver's constellations, and it does so **silently**:
nothing reports an error, there are simply fewer satellites and worse geometry, which is
exactly what hurts at a circuit surrounded by grandstands and pit buildings.

The AA.170 covers all three bands explicitly.

### Electrical fit

- **1.8 V min, 3.0 V typ, 5.5 V max** - works on the `GPS-17285`'s 3.3 V antenna bias
- **26-32 dB LNA** - covers the roughly 3 dB loss of its 3 m RG-174 without over-driving the
  M9N front end
- **SMA(M)** - mates directly with the breakout's SMA, with no adaptor and no U.FL pigtail
- **IP67**, manufactured to IATF 16949

### Mounting matters more than the antenna does

- **It needs a ground plane**, which is what the magnet mount uses the steel roof for. Mount it
  on the roof, centred, with a clear view of the sky.
- **Do not mount it inside the cabin.** An antenna on the dash under the windscreen loses
  signal to the glass and gains multipath from the screen and A-pillars, which is the error
  this whole selection exists to reduce.
- **Retention at track speed.** These magnets are strong and the part is used on commercial
  vehicles, but an antenna leaving a car at 120 mph is a hazard to everyone else on circuit.
  Fit a safety tether, or use the **AA.171**, the same antenna in an adhesive and screw-mount
  body, once the pod is a permanent fit.

### Considered and rejected

| Candidate | Coverage | UK price inc VAT | Outcome |
|---|---|---:|---|
| **Taoglas AA.170** | 1561 / 1575 / 1602 | **£26.58** | **Selected** |
| SparkFun `GPS-14986` | 1575-1610 | ~£18 | Rejected: misses BeiDou |
| SparkFun `GPS-15192` / u-blox ANN-MB-00 | 1559-1606 and 1197-1249 | £98.84 | Rejected: L2 band unusable on an L1 receiver, four times the price |

### Receiver packaging: why the MicroMod and MAX-M10S boards are not substitutes

Both are stocked by The Pi Hut and are easier to buy than the selected part. Neither works here.

**SparkFun MicroMod GNSS Function Board, NEO-M9N, £41.30.** Same receiver, wrong packaging. It
terminates in an **M.2 edge connector** and its listing states it does not include a MicroMod
Main Board, so it cannot reach the Waveshare board without buying a carrier whose purpose is to
host a MicroMod processor. Its antenna connector is **U.FL**, which reintroduces exactly what
selecting the SMA variant removed: a fragile joint, rated for few mating cycles, in a
vibrating car, plus a U.FL-to-SMA pigtail to reach the selected antenna. With a carrier and a
pigtail it costs about the same as `GPS-17285` and adds two failure points.

**SparkFun MAX-M10S breakout.** Has the SMA connector and Qwiic, and fails on rate: **10 Hz
with four constellations**, half the minimum. See the table above.

**Availability.** Pimoroni no longer stock `GPS-17285`, and The Pi Hut do not carry it. DigiKey
UK remains the source, with SparkFun direct as the alternative.


### Alternatives considered and rejected, 2026-08-21

Re-examined against the possibility of better reception. The M9N was retained. Recorded here
so the question is not reopened from scratch.

| Candidate | Bands | Max navigation rate | Protocol | UK price inc VAT | Outcome |
|---|---|---|---|---|---|
| **NEO-M9N `GPS-17285`** | L1 | **25 Hz, four constellations** | UBX | **£69.59** | **Retained** |
| NEO-F10N | L1 + L5 | 10 Hz | UBX | ~£60 | Rejected: rate |
| SparkFun LG290P quad-band RTK | quad | 20 Hz | NMEA 0183 / RTCM | £182.40 | Rejected: protocol, RTK |
| u-blox ZED-F9P `GPS-16481` | L1 + L2 | 9 to 25 Hz, configuration-dependent | UBX | ~£205 | Not proven to meet the rate requirement |

**NEO-F10N** is dual-band and inexpensive, and fails outright at **10 Hz** against a 20 Hz
minimum.

**LG290P** fails on two counts. It speaks NMEA 0183 and RTCM but **not UBX**, and standard NMEA
carries time to 10 ms where UBX carries it to the nanosecond - the crossing-time interpolation
in `crossing_time.cpp` exists specifically to avoid quantising a lap to the update grid, and a
10 ms timestamp reintroduces a large part of what it removes. Separately, its headline feature
is RTK, which this project lists as an explicit non-goal and could not use anyway: RTK needs a
correction stream, and the device must work fully offline with no dependency on Wi-Fi or a
phone.

**ZED-F9P** is the only genuinely tempting one: multi-band L1/L2, native UBX, and real
multipath rejection, which is the dominant position error at a circuit. It is **not confirmed
to meet the rate requirement**. Its data sheet quotes the maximum navigation rate as a table of
six values by constellation configuration - PVT 9 / 10 / 20 / 20 / 16 / 25 Hz, RTK 7 / 10 / 15 /
14 / 13 / 20 Hz - and the pattern indicates that the higher rates come from running fewer
constellations. If 25 Hz means GPS alone, then at full four-constellation concurrency the rate
may fall below the 20 Hz minimum, and the concurrency being given up is itself a reception
benefit. **Anyone revisiting this must confirm which configuration yields which rate before
ordering.**

## The open reception decision is the antenna, not the receiver

Line H3 is still "to be selected", and it is now the highest-value reception decision left.
Multipath dominates position error at circuits, and antenna quality, ground plane and mounting
position dominate multipath. A good active L1 antenna properly mounted will improve reception
more per pound than any receiver in the table above, and a poor one would waste a £205 module
entirely.

If trace logging later shows multipath limiting lap repeatability, the ZED-F9P or the
dead-reckoning ZED-F9R become evidence-backed upgrades rather than guesses - the logging needed
to make that judgement already exists.

### Why the SMA variant is now the baseline

The previously proposed `GPS-15712` U.FL breakout has been superseded in this project by `GPS-17285`.

Benefits:

- eliminates the fragile U.FL connector from the prototype antenna path
- removes the U.FL-to-SMA pigtail from the BOM
- gives a mechanically stronger antenna connection for a vibration-prone track car
- simplifies the rear-pod enclosure because the breakout's SMA connector can be made directly accessible

Do not substitute a visually similar NEO-M9N board without checking the exact u-blox module, update-rate capability, logic voltage and antenna interface.

## D. GNSS antenna

The antenna is deliberately **not yet frozen**. It should be selected from a UK procurement route after the GPS-17285 has been obtained and the intended mounting position has been confirmed.

Required characteristics:

- SMA male connection compatible with the GPS-17285
- active 50 ohm GNSS antenna unless testing demonstrates a passive antenna is preferable
- coverage appropriate for the M9N L1-region constellation set used by the firmware
- suitable active-antenna bias requirements for the receiver/breakout
- weather-resistant external mounting
- secure mechanical mounting suitable for track use
- cable length appropriate to the car without unnecessary extensions or coils
- documented operating-temperature range

Antenna placement is part of the timing system. It should have the clearest practical view of the sky and should not be buried behind the display, driver, metalwork or electrically noisy power hardware.

## E. GNSS power

Do **not** assume the Waveshare 3.3 V rail is adequate for every receiver and active-antenna transient until measured on the assembled prototype.

During hardware bring-up:

1. confirm the available Waveshare rail/current capability from its schematic
2. measure GPS-17285 cold-start and tracking current
3. measure the rail with the display at maximum brightness
4. repeat while microSD logging is active
5. include active-antenna current in the test
6. add a dedicated low-noise 3.3 V regulator from the protected 5 V input if required

A later custom daughterboard should include deliberate GNSS supply filtering and RF/ESD considerations.

## F. Prototype connection

Baseline electrical connection is a dedicated UART:

```text
Waveshare ESP32-S3                     SparkFun GPS-17285
------------------                     ------------------
3.3 V / validated supply  -----------> VCC
GND                       -----------> GND
UART TX                    -----------> RX
UART RX                    <----------- TX
optional GPIO              <----------- TIMEPULSE / PPS

GPS-17285 SMA              <---------- external active GNSS antenna
```

Exact ESP32-S3 GPIO assignments must be taken from the Waveshare schematic and verified against display, touch, IMU, RTC, microSD and USB use before firmware pins are frozen.

## G. Battery

Battery operation is optional for the first prototype. Bench USB and then a protected/fused 5 V vehicle supply are simpler for initial development.

If a Li-ion/LiPo pack is fitted:

- use only the voltage range and connector/polarity specified by Waveshare
- source the cell from a reputable UK supplier
- use a protected cell where appropriate
- verify charging behaviour in the actual enclosure
- provide strain relief so vibration cannot load the battery connector
- do not leave an unknown or damaged cell charging unattended in a hot vehicle

## H. Mounting and enclosure

The prototype should retain the Waveshare `-B` front case and add a small rear GNSS pod.

The rear pod should:

- positively retain the GPS-17285 board
- allow the board's SMA connector to be accessed without loading the PCB
- provide cable strain relief
- preserve access to USB-C and microSD where practical
- provide a secure vehicle-mount interface
- prevent loose hardware under vibration

Avoid relying on adhesive alone for final track use.

## I. Alternative GNSS options

Keep these as fallback research, not parallel implementation targets:

| Option | Advantage | Disadvantage |
|---|---|---|
| NEO-M9N custom daughterboard | Best packaging | RF and manufacturing work is premature before software and antenna validation |
| modern dual-frequency receiver | Better positioning potential | Higher cost, power and scope than the MVP requires |
| 10 Hz integrated GNSS/display board | Simple packaging | Fails the mandatory 20 Hz requirement |

## J. UK procurement rule

For this project:

1. record prices in GBP
2. identify whether figures include or exclude VAT
3. prefer UK suppliers or UK-facing distributor sites
4. store a direct procurement URL in the BOM
5. recheck stock and price immediately before purchase
6. avoid undocumented marketplace GNSS boards where module authenticity is uncertain
7. once hardware bring-up starts, buy enough core hardware for **two prototypes** where budget permits, keeping one as an unmodified known-good reference
