# ADR-004: Use directional virtual-line intersection with timestamp interpolation

**Status:** Accepted

## Rejected alternative

A radius/geofence trigger around the start point is too sensitive to path geometry and can create ambiguous repeat crossings.

## Decision

Represent start/finish as a line segment with an allowed crossing direction. Detect intersection of consecutive GNSS movement segments and interpolate crossing time between receiver measurement timestamps.

## Consequences

- better-defined event geometry
- timing resolution is not quantised to the next GNSS output epoch
- requires careful rearm, direction and quality logic
- remains limited by GNSS position error
