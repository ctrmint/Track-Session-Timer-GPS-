# Hardware Integration

## 1. Prototype packaging

Phase 1 should keep the Waveshare `-B` case intact as much as possible. Add a small rear or side pod for:

- NEO-M9N breakout
- U.FL cable retention
- SMA bulkhead connector
- optional dedicated GNSS regulator/filtering
- wiring connector to the Waveshare UART and power pins

This reduces risk while the screen is still being evaluated.

## 2. GNSS UART

Preferred prototype connection:

```text
ESP32-S3 TX  -> M9N RX
ESP32-S3 RX  <- M9N TX
ESP32-S3 GND -- M9N GND
3.3 V supply -> M9N VCC
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

115200 is likely sufficient for a small set of 25 Hz UBX messages, but 230400 provides margin for diagnostics. The actual byte rate must be measured.

## 4. GNSS message set

Minimum desired information:

- PVT fix containing GNSS time, position, speed, heading, fix type, satellite count and accuracy estimates
- receiver status required to reject invalid fixes
- optional timepulse/PPS for timing diagnostics

Disable unnecessary high-rate text/NMEA output once bring-up is complete. This reduces UART and parser load.

## 5. RF rules

- Keep U.FL connections internal and mechanically retained.
- Use SMA or another robust connector at the enclosure boundary.
- Keep GNSS RF cabling away from noisy DC/DC conversion where practical.
- Do not coil unnecessary antenna cable beside the ESP32 antenna/display electronics.
- Provide ESD protection in a custom GNSS daughterboard design.
- Treat external antenna placement as part of the timing system, not an accessory decision.

## 6. Power

Development order:

1. board powered by a bench USB supply
2. GNSS powered separately if necessary during early diagnostics
3. common 5 V/3.3 V design after current measurement
4. vehicle USB supply test
5. optional internal battery test

Record current draw at:

- boot
- maximum display brightness
- GNSS cold start
- GNSS tracking at 25 Hz
- SD continuous write
- Wi-Fi off and on, even though Wi-Fi is not used during normal track operation

## 7. EMI and Wi-Fi

Normal track mode should not require Wi-Fi. If GNSS sensitivity changes materially with Wi-Fi enabled, disable Wi-Fi during active timing and document the result.

## 8. Enclosure environment

Design for:

- vibration
- direct sun
- cockpit heat after parking
- cable pull
- gloved fingers
- quick removal from the vehicle
- access to USB-C and microSD without dismantling the whole unit if practical

The final case should have a small visor or screen angle option if sunlight testing shows it is beneficial.
