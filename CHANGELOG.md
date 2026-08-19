# Changelog

All notable changes to this project will be documented here.

The project follows Semantic Versioning once the first firmware release is tagged.

## Unreleased

### Fixed

- the scaled carousel icon drove LVGL's software image transform hard enough to starve
  the UI task, tripping the task watchdog and freezing the menu
- the G meter showed raw accelerometer axes, so the dot sat wherever the unit was tilted
  instead of at the centre
- a swipe also fired a press, because LVGL sends RELEASED before SHORT_CLICKED
- swipes starting on a clickable child never reached the screen's gesture handler

- RM690B0 partial redraws rendered as offset horizontal bands; flush areas are now
  aligned to even columns

### Added

- Waveshare board hardware configuration: 16 MB flash, 8 MB octal PSRAM, 240 MHz CPU
- RM690B0 AMOLED panel bring-up over QSPI with LVGL 9 display registration
- FT6336 touch input registered as an LVGL pointer device
- on-device screen routing for the ready, setup, review and diagnostics screens
- microSD mount, inspection and opt-in format support
- QMI8658 6-axis IMU driver on a shared board I2C bus
- gravity auto-calibration: centres the G meter at any mounting angle and corrects
  sensor scale
- radar-style G meter used as the G-Only display
- device settings persisted in NVS, so Mode survives a reboot
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
