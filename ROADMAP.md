# Product Roadmap

## Release 1.0: dependable track timer

Required:

- 20 Hz minimum GNSS operation
- directional start/finish line crossing
- interpolated crossing timestamp
- current/previous/best lap
- original session countdown workflow
- session logging to microSD
- track definitions stored locally
- external GNSS antenna
- readable high-contrast UI
- graceful GNSS, IMU and storage degradation

## Release 1.1: better lap analysis

Candidates:

- sectors
- theoretical best
- lap consistency summary
- speed at line
- max/min speed summaries
- improved post-session review
- GPX/CSV export tooling

## Release 1.2: live delta

Candidates:

- best-lap reference trace
- distance-along-lap mapping
- live delta display
- delta bar with restrained colour cues
- validity checks when the car deviates substantially from the reference path

Live delta must be implemented only after normal lap timing is proven reliable.

## Release 1.3: easier track management

Candidates:

- USB or Wi-Fi track database update while stationary
- browser-based configuration page
- import/export track files
- automatic nearest-track suggestion
- track pack repository

## Future research

- 50 Hz or higher GNSS receivers
- dual-frequency GNSS
- dead reckoning / sensor fusion
- external wheel button
- haptic or audible alert
- pit display link
- BLE export
- optional CAN input as a separate expansion, not a core dependency

## Non-roadmap principle

The device remains useful without cloud access, mobile reception or a phone.
