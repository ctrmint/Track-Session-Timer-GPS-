# Source References

Hardware planning sources checked on **2026-08-12**.

These links are references for procurement and engineering. Recheck current revisions before freezing a PCB or production BOM.

## Waveshare display/controller

- Product: https://www.waveshare.com/esp32-s3-touch-amoled-2.41.htm
- Wiki: https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.41

Verified planning points:

- ESP32-S3R8
- 8 MB PSRAM, 16 MB flash
- 600 x 450 AMOLED
- RM690B0
- FT6336 touch
- 800 cd/m2 stated brightness
- QMI8658
- RTC and TF/microSD listed
- `-B` variant includes case
- ESP-IDF and Arduino support advertised

## u-blox NEO-M9N

- Product: https://www.u-blox.com/en/product/neo-m9n-module
- Integration manual: https://content.u-blox.com/sites/default/files/NEO-M9N_Integrationmanual_UBX-19014286.pdf

Planning points:

- M9 platform supports up to 25 Hz position update rate
- active/passive antenna integration guidance
- UART interface
- external active antenna supply/control capability
- integration manual notes M9N peak current around 100 mA

## SparkFun NEO-M9N breakout

- Product (DigiKey UK, the sourcing selected in `HARDWARE_BOM.md`): https://www.digikey.co.uk/en/products/detail/sparkfun-electronics/GPS-17285/13561758
- Hookup guide: https://learn.sparkfun.com/tutorials/sparkfun-gps-neo-m9n-hookup-guide

Planning points:

- SKU GPS-17285, the **SMA** variant
- 25 Hz max update rate, across all five constellation configurations including
  GPS+GLO+GAL+BDS - the rate does not fall as constellations are added
- 3.3 V VCC/I/O
- UBX/NMEA/RTCM over UART or I2C. Both of this board's connectors are Qwiic (I2C); its
  UART is plated through-holes only, so a UART link means soldering four wires
- Module default is 38400 baud 8N1. The 115200 in the hookup guide is their serial
  monitor's setting, not the receiver's

**Superseded:** `GPS-15712`, the U.FL variant, with an SMA-to-U.FL pigtail (`WRL-09145`).
Dropped to keep a fragile U.FL joint out of a vibrating car. See `HARDWARE_BOM.md`.

## Antenna

The selected antenna is in `HARDWARE_BOM.md` (H3): **Taoglas Magma X `AA.170.301111`**.

**Do not substitute on price alone.** The M9N tracks four constellations concurrently at
different frequencies - GPS and Galileo at 1575.42 MHz, GLONASS L1 at 1602 +/- 8 MHz, and
**BeiDou B1 at 1561.098 MHz**. Many antennas sold as "GPS/GNSS" are tuned 1575-1610 MHz
and sit entirely above BeiDou, which costs a quarter of the available constellations with
no error, no warning and no obvious symptom beyond slightly worse fixes.

**Superseded:** GPS/GNSS Magnetic Mount Antenna `GPS-14986`
(https://www.sparkfun.com/gps-gnss-magnetic-mount-antenna-3m-sma.html). It was the
low-cost development reference, and it is one of the 1575-1610 MHz parts described above.
Rejected for that reason, not on quality.

## Firmware/UI

- ESP-IDF documentation: https://docs.espressif.com/projects/esp-idf/
- LVGL ESP32/ESP-IDF integration: https://docs.lvgl.io/master/integration/chip_vendors/espressif/add_lvgl_to_esp32_idf_project.html
