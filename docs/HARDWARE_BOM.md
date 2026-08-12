# Hardware Bill of Materials

Baseline checked: **2026-08-12**.

Prices are indicative vendor prices in USD before tax, shipping and import charges. Availability and pricing should be rechecked before purchase.

## A. Core prototype BOM

| Ref | Qty | Item | Part / SKU | Baseline price | Why selected |
|---|---:|---|---|---:|---|
| H1 | 1 | Waveshare ESP32-S3-Touch-AMOLED-2.41-B | ESP32-S3-Touch-AMOLED-2.41-B | about $53.99 | 2.41 inch 600 x 450 AMOLED, stated 800 cd/m2, touch, ESP32-S3R8, 8 MB PSRAM, 16 MB flash, QMI8658, RTC, TF/microSD and supplied case |
| H2 | 1 | SparkFun NEO-M9N U.FL breakout | GPS-15712 | $70.95 | Proven NEO-M9N breakout, 25 Hz max, 3.3 V logic, UART/I2C, U.FL antenna connection |
| H3 | 1 | U.FL to SMA female cable/pigtail | WRL-09145 or equivalent panel-mount version | $5.75 reference | Moves the fragile U.FL connection inside the enclosure and provides a robust external antenna connector |
| H4 | 1 | Active GNSS antenna | See antenna section | $16 to $110 typical | External sky-facing antenna is strongly preferred in a Caterham-style cockpit |
| H5 | 1 | High-endurance microSD | 8 to 32 GB, FAT32 during development | market price | Session trace and event logging |
| H6 | 1 | USB-C data/power cable | quality short cable | market price | Programming and bench power |
| H7 | 1 | 5 V vehicle USB supply | fused, good-quality automotive adaptor | market price | Simplest prototype vehicle power path |
| H8 | 1 | Rear GNSS enclosure/pod | 3D printed prototype | fabrication | Holds GNSS breakout, cable strain relief and antenna connection behind/adjacent to the Waveshare case |
| H9 | as needed | M2/M3 fasteners, inserts, spacers | stainless/brass | market price | Secure vibration-resistant assembly |

**Core electronic subtotal using the low-cost antenna reference:** approximately **$147.19**, before SD card, power adaptor, enclosure, tax and shipping.

## B. Display/controller board

### Selected: Waveshare ESP32-S3-Touch-AMOLED-2.41-B

Verified vendor characteristics:

- ESP32-S3R8
- 8 MB PSRAM
- 16 MB flash
- 600 x 450 AMOLED
- RM690B0 display driver
- FT6336 capacitive touch
- stated brightness 800 cd/m2
- QMI8658 IMU
- RTC
- TF/microSD support
- I2C, UART, USB and 34-pin GPIO header
- `-B` variant supplied with case

Why not use the smaller 1.75 inch board:

- this rebuild is intended to materially improve readability
- 2.41 inches gives more room for session time plus lap timing without making critical text tiny
- the 600 x 450 panel supports a much more flexible layout

Why not automatically choose a larger 3.5 inch LCD:

- physical size is not the only criterion for an open cockpit
- brightness and contrast matter heavily
- the 2.41 inch AMOLED is a good first prototype balance of compact packaging and visibility

The screen choice remains a prototype gate. A direct sunlight cockpit test is required before freezing the enclosure.

## C. GNSS receiver

### Selected prototype: SparkFun GPS Breakout NEO-M9N, U.FL (Qwiic), GPS-15712

Vendor-listed capabilities include:

- u-blox NEO-M9N
- 25 Hz maximum navigation update rate
- GPS, GLONASS, Galileo and BeiDou capable receiver
- UART and I2C interfaces
- NMEA, UBX and RTCM protocol support
- 3.3 V VCC and I/O
- U.FL external antenna connector
- timepulse capability

The firmware target is **20 Hz minimum**. 25 Hz should be tested and used if the full system remains stable.

### Production direction

The breakout is appropriate for development, but the intended final packaging should use either:

1. a dedicated GNSS daughterboard containing the NEO-M9N and RF protection, or
2. a mechanically secure commercial M9N module assembly.

Do not design a custom RF PCB until the software and antenna installation have been validated using the breakout.

## D. Antenna

The u-blox integration guidance makes antenna quality and RF layout a first-order system concern. The M9N can work with passive antennas but supports an active antenna, which is preferred when the antenna is separated from the receiver and mounted for a clear sky view.

### Development budget option

SparkFun GPS/GNSS Magnetic Mount Antenna, GPS-14986, about $16.50.

This is useful for initial GPS/GLONASS testing and provides a magnetic vehicle mount. It should not be treated as the final multi-constellation antenna specification without checking frequency coverage against the constellations enabled in the receiver.

### Recommended final antenna specification

Select an active 50 ohm antenna that:

- covers the GNSS bands used by the configured M9N constellation set
- is compatible with a 3.3 V bias or has a suitable external filtered supply
- has a secure automotive-friendly mount
- has a cable length appropriate for the car without unnecessary extension
- has documented operating temperature and weather resistance

The u-blox NEO-M9N integration manual identifies the relevant L1-region coverage as approximately 1559 to 1606 MHz across supported constellations and discusses active antenna bias and protection.

## E. GNSS power

Do **not** assume the Waveshare 3.3 V rail can safely supply every GNSS/active-antenna peak without measurement.

The NEO-M9N integration manual notes a receiver peak current around 100 mA. An active antenna can add additional current. During M2 hardware bring-up:

1. verify the Waveshare exposed rail and regulator capability from schematic/documentation
2. measure startup and tracking current
3. measure 3.3 V rail droop while the display is at high brightness and SD is writing
4. add a dedicated 3.3 V low-noise regulator from the 5 V input if required

A custom daughterboard should include deliberate GNSS power filtering.

## F. Battery

Battery operation is optional for the first prototype. Vehicle USB power is simpler for initial development.

If a Li-ion/LiPo pack is fitted:

- use only the voltage range and connector/polarity specified by Waveshare
- use a protected cell from a reputable supplier
- verify charging behaviour in the actual enclosure
- do not leave an unknown cell charging unattended in a hot vehicle
- provide strain relief so vibration cannot load the battery connector

## G. Mounting

The enclosure/mount must prevent the unit becoming a projectile and must not obstruct vehicle controls or driver vision.

Prototype options:

- 3D printed rear pod bolted to the Waveshare case mounting points
- separate GNSS pod fixed directly behind the display
- RAM/GoPro-style external mounting interface attached to the rear pod

Avoid relying on adhesive alone for final track use.

## H. Alternative GNSS options

Keep these as fallback research, not parallel implementation targets:

| Option | Advantage | Disadvantage |
|---|---|---|
| NEO-M9N SMA breakout | More robust antenna connector on the breakout | Larger/mechanically harder to integrate inside a compact case |
| NEO-M9N custom board | Best packaging | RF design and manufacturing work too early in project |
| modern dual-frequency receiver | Better positioning potential | More cost, power and software scope than MVP needs |
| 10 Hz integrated GNSS display board | Very simple | Fails the 20 Hz requirement |

## I. Procurement rule

Buy enough hardware for **two prototypes** once M1/M2 bring-up begins. One unit can remain a known-good reference while the other is modified for enclosure and vehicle work.
