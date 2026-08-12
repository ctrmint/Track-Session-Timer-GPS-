# Hardware Integration

Baseline checked: **2026-08-12**.

The selected prototype hardware is:

- Waveshare `ESP32-S3-Touch-AMOLED-2.41-B`, preferably procured in the UK from The Pi Hut as SKU `WAV-30589`
- SparkFun `GPS-17285` NEO-M9N **SMA** breakout, preferably procured through DigiKey UK as part `1568-17285-ND`

The old `GPS-15712` U.FL breakout and U.FL-to-SMA pigtail are **not** part of the current baseline.

## 1. Prototype packaging

Phase 1 should keep the Waveshare `-B` case intact as much as possible. Add a small rear or side pod for:

- SparkFun GPS-17285 NEO-M9N breakout
- direct access to, or mechanical protection around, the breakout's SMA antenna connector
- optional dedicated GNSS regulator/filtering
- wiring to the Waveshare UART and validated power rail
- strain relief for internal wiring
- vehicle mounting interface

This approach reduces enclosure work while the display, GNSS and antenna installation are still being validated.

### UK procurement references

- Waveshare board, The Pi Hut: https://thepihut.com/collections/waveshare/products/esp32-s3-2-41-amoled-touch-display-dev-board-with-case-600x450
- SparkFun GPS-17285, DigiKey UK: https://www.digikey.co.uk/en/products/detail/sparkfun-electronics/GPS-17285/13561758

Procurement data belongs in `HARDWARE_BOM.md` and `hardware/bom.csv`; prices and stock must be rechecked before ordering.

## 2. GNSS UART

Preferred prototype connection:

```text
ESP32-S3 TX  -> GPS-17285 / M9N RX
ESP32-S3 RX  <- GPS-17285 / M9N TX
ESP32-S3 GND -- GPS-17285 GND
3.3 V supply -> GPS-17285 VCC
optional GPIO <- M9N TIMEPULSE/PPS
```

Final GPIO numbers are deliberately **not assigned in this planning package**. They must be selected from the Waveshare schematic after confirming which pins are unused by display, touch, IMU, SD, RTC and USB functions.

Do not copy pin assignments from another Waveshare board with a similar name.

## 3. UART configuration

Initial target:

- 230400 baud
- 8 data bits
- no parity
- 1 stop bit
- UBX binary messages only during normal operation where practical

115200 may be sufficient for a deliberately small set of 20/25 Hz UBX messages, but 230400 gives useful margin for diagnostics and future message additions. Measure actual serial throughput rather than relying only on calculation.

## 4. GNSS update-rate bring-up

Bring the receiver up in this order:

1. establish a valid fix at the default receiver configuration
2. verify UBX parsing and GNSS measurement timestamps
3. configure **20 Hz** and run sustained bench logging
4. run 20 Hz with display updates, IMU and microSD logging concurrently
5. evaluate **25 Hz** only after 20 Hz is proven stable
6. record dropped, late, invalid and out-of-order fixes as diagnostics

The project requirement is **20 Hz minimum**. The 25 Hz capability is useful headroom, not permission to compromise reliability.

## 5. GNSS message set

Minimum desired information:

- PVT fix containing GNSS time, position, speed, heading, fix type, satellite count and accuracy estimates
- receiver status required to reject invalid fixes
- optional timepulse/PPS for timing diagnostics

Disable unnecessary high-rate NMEA text output once bring-up is complete. Use UBX binary messages for the normal timing path where practical to reduce bandwidth and parsing overhead.

## 6. RF and antenna rules

The selected GPS-17285 already provides an integrated SMA antenna connector.

Therefore:

- **do not add a U.FL-to-SMA pigtail to the baseline design**
- design the rear pod so the SMA connector is accessible without the external antenna cable applying bending load to the PCB
- if enclosure geometry eventually requires a short SMA extension or bulkhead, add it deliberately and document the reason
- keep GNSS RF cabling away from noisy DC/DC conversion where practical
- do not coil unnecessary antenna cable beside ESP32/display electronics
- treat antenna placement as part of the timing system, not as an accessory decision
- provide appropriate ESD/RF protection if the design later moves to a custom GNSS daughterboard

The final active antenna is not yet selected. It must use a compatible SMA connection and be sourced through a suitable UK procurement route.

## 7. Power

Development order:

1. Waveshare board powered by a bench USB supply
2. GPS-17285 powered from a separately monitored 3.3 V source if useful for early current measurements
3. validate whether the Waveshare 3.3 V rail can power the receiver plus active antenna under worst-case system load
4. add dedicated low-noise GNSS regulation if required
5. test with a fused/protected 5 V vehicle USB supply
6. evaluate optional internal battery operation only after the wired system is stable

Record current and rail voltage at:

- boot
- maximum display brightness
- GNSS cold start
- GNSS tracking at 20 Hz
- GNSS tracking at 25 Hz
- SD continuous write
- combined display + GNSS + SD load
- Wi-Fi off and on, even though Wi-Fi is not required during normal track operation

## 8. EMI and Wi-Fi

Normal track mode should not require Wi-Fi. If GNSS sensitivity changes materially with Wi-Fi enabled, disable Wi-Fi during active timing and document the measured result.

Do not place the GNSS antenna feed directly alongside noisy power conversion or unnecessary high-speed digital wiring.

## 9. Enclosure environment

Design for:

- vibration
- direct sunlight
- cockpit heat after parking
- antenna cable pull
- gloved fingers
- quick removal from the vehicle
- access to USB-C and microSD without dismantling the whole unit where practical
- secure support of the GPS-17285 so the SMA connector is not used as a structural mounting point

The final case may benefit from a small visor or an adjustable screen angle if sunlight testing demonstrates a real improvement.

## 10. Hardware acceptance gates

Before moving to track validation, the prototype must demonstrate:

- reliable display/touch operation at intended brightness
- stable 20 Hz GNSS reception and parsing for an extended bench run
- no loss of GNSS data while microSD logging is active
- clean power rails under combined load
- mechanically secure SMA and power connections
- repeatable hot/warm GNSS startup behaviour
- no enclosure-induced reset or connector movement under vibration testing
- antenna position that provides consistently usable satellite reception in the intended car installation
