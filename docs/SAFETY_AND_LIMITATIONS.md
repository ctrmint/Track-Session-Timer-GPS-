# Safety and Limitations

## Intended use

This project is a driver aid for track-day or similar closed-course use. It is not a certified timing system and is not a replacement for an official race transponder where regulations require one.

## Driver interaction

Do not configure the device while driving. The active-session UI should be designed so useful information can be read without prolonged attention.

Mount the device so that it:

- cannot interfere with steering or controls
- does not obstruct the driver's required view
- cannot become a loose projectile
- does not place a rigid object in an unsafe impact location

## Timing limitations

A 20/25 Hz GNSS receiver gives frequent measurements but does not provide centimetre-level position. Lap timing error depends on:

- satellite geometry
- multipath
- antenna placement
- receiver configuration
- start-line geometry
- vehicle speed and path
- position noise at the line

Interpolation improves crossing-time resolution between fixes. It does not remove position uncertainty.

## Electrical

Use appropriate fused vehicle power. Do not connect raw 12 V to pins intended for 5 V or 3.3 V.

Battery charging inside a hot cockpit requires particular care and should use only the board/vendor-supported battery arrangement.
