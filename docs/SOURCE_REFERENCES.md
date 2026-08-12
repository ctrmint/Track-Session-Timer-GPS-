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

## SparkFun NEO-M9N development breakout

- Product: https://www.sparkfun.com/sparkfun-gps-breakout-neo-m9n-u-fl-qwiic.html
- Hookup guide: https://learn.sparkfun.com/tutorials/sparkfun-gps-neo-m9n-hookup-guide

Planning points:

- SKU GPS-15712
- U.FL connector
- 25 Hz max update rate
- 3.3 V VCC/I/O
- UBX/NMEA/RTCM over UART or I2C

## Reference antenna and pigtail

- GPS/GNSS Magnetic Mount Antenna, GPS-14986: https://www.sparkfun.com/gps-gnss-magnetic-mount-antenna-3m-sma.html
- SMA to U.FL cable, WRL-09145: https://www.sparkfun.com/interface-cable-sma-to-u-fl.html

The low-cost magnetic antenna is a development reference, not a final multi-constellation antenna specification.

## Firmware/UI

- ESP-IDF documentation: https://docs.espressif.com/projects/esp-idf/
- LVGL ESP32/ESP-IDF integration: https://docs.lvgl.io/master/integration/chip_vendors/espressif/add_lvgl_to_esp32_idf_project.html
