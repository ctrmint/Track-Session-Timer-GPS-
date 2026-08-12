"""Host-side reference geometry for early lap timing tests.

This is intentionally small and dependency-free. Production embedded code can use
a different implementation, but expected intersection behaviour should match these
tests.
"""

from __future__ import annotations
from dataclasses import dataclass


@dataclass(frozen=True)
class Point:
    x: float
    y: float


def cross(a: Point, b: Point) -> float:
    return a.x * b.y - a.y * b.x


def subtract(a: Point, b: Point) -> Point:
    return Point(a.x - b.x, a.y - b.y)


def segment_intersection_fraction(p0: Point, p1: Point, a: Point, b: Point):
    """Return fraction u along p0->p1 if segments intersect, else None.

    Collinear overlap is deliberately returned as None because a car travelling
    along the timing line is not a well-defined crossing event.
    """
    r = subtract(p1, p0)
    s = subtract(b, a)
    denom = cross(r, s)
    if abs(denom) < 1e-12:
        return None

    qmp = subtract(a, p0)
    u = cross(qmp, s) / denom
    v = cross(qmp, r) / denom
    if 0.0 <= u <= 1.0 and 0.0 <= v <= 1.0:
        return u
    return None


def interpolate_time(t0_s: float, t1_s: float, fraction: float) -> float:
    if not 0.0 <= fraction <= 1.0:
        raise ValueError("fraction must be between 0 and 1")
    if t1_s < t0_s:
        raise ValueError("timestamps must be monotonic")
    return t0_s + fraction * (t1_s - t0_s)
