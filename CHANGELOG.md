# Changelog

All notable changes to this project will be documented here.

The project follows Semantic Versioning once the first firmware release is tagged.

## Unreleased

### Fixed

- RM690B0 partial redraws rendered as offset horizontal bands; flush areas are now
  aligned to even columns

### Added

- Waveshare board hardware configuration: 16 MB flash, 8 MB octal PSRAM, 240 MHz CPU
- RM690B0 AMOLED panel bring-up over QSPI with LVGL 9 display registration
- FT6336 touch input registered as an LVGL pointer device
- on-device screen routing for the ready, setup, review and diagnostics screens
- microSD mount, inspection and opt-in format support
- press-and-hold gated menu with swipe carousels for Setup, Review and Diagnostics
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
