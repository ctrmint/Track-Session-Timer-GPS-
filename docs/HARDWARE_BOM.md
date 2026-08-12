# Hardware Bill of Materials

Baseline checked: **2026-08-12**.

This is a **UK project**. Prototype procurement should therefore use UK-facing suppliers where practical, prices should be recorded in **GBP**, and VAT should be shown explicitly. Overseas direct purchase should be treated as a fallback rather than the baseline procurement route.

Prices and stock below are a point-in-time reference only and must be rechecked before ordering.

## A. UK procurement baseline

| Ref | Qty | Item | Manufacturer part / SKU | Preferred UK supplier | Supplier reference | Baseline UK price | Procurement link | Status / reason |
|---|---:|---|---|---|---|---:|---|---|
| H1 | 1 | Waveshare ESP32-S3 2.41 inch AMOLED Touch Display Dev Board **with case** | `ESP32-S3-Touch-AMOLED-2.41-B` | The Pi Hut | `WAV-30589` | **£51.90 inc VAT** | https://thepihut.com/collections/waveshare/products/esp32-s3-2-41-amoled-touch-display-dev-board-with-case-600x450 | **Selected**. 600 x 450 AMOLED, 800 cd/m2 stated brightness, touch, ESP32-S3R8, 8 MB PSRAM, 16 MB flash, QMI8658 IMU, RTC, microSD/TF and supplied protective case. |
| H2 | 1 | SparkFun GPS Breakout, u-blox NEO-M9N, **SMA** (Qwiic) | `GPS-17285` / manufacturer part `17285` | DigiKey UK | `1568-17285-ND` | **£57.99 ex VAT / £69.59 inc VAT** | https://www.digikey.co.uk/en/products/detail/sparkfun-electronics/GPS-17285/13561758 | **Selected**. Genuine NEO-M9N breakout, 25 Hz max navigation rate, 3.3 V logic, UART/I2C, UBX support and integrated SMA antenna connector. |
| H3 | 1 | Active GNSS antenna, SMA male | To be selected | UK supplier required | TBD | TBD | TBD | **Required before vehicle testing**. Must be appropriate for the M9N L1 GNSS bands, active-antenna bias and external automotive mounting. |
| H4 | 1 | High-endurance microSD | 8 to 32 GB | UK supplier | TBD | TBD | TBD | Session trace and event logging. FAT32 during development. |
| H5 | 1 | USB-C data/power cable | quality short cable | UK supplier | TBD | TBD | TBD | Programming and bench power. |
| H6 | 1 | 5 V vehicle USB supply | fused, good-quality automotive adaptor | UK supplier | TBD | TBD | TBD | Prototype vehicle power path. Do not connect raw vehicle 12 V to the Waveshare board. |
| H7 | 1 | Rear GNSS enclosure/pod | 3D printed prototype | UK fabrication / in-house | custom | TBD | n/a | Holds the GPS-17285 securely and exposes/protects the SMA antenna connection. |
| H8 | as needed | M2/M3 fasteners, threaded inserts and spacers | stainless/brass | UK supplier | TBD | TBD | TBD | Vibration-resistant assembly. |

**Known selected-electronics subtotal:** **£121.49 inc VAT** for H1 + H2 at the checked single-unit prices. This excludes antenna, microSD, power, enclosure fabrication and delivery.

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

The firmware acceptance requirement remains **20 Hz minimum**. Development should establish reliable 20 Hz operation first, then evaluate 25 Hz with the display, SD logging and all normal tasks running concurrently.

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
