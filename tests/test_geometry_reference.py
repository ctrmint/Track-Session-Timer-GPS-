import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from geometry_reference import Point, interpolate_time, segment_intersection_fraction


class GeometryReferenceTests(unittest.TestCase):
    def test_perpendicular_midpoint_crossing(self):
        u = segment_intersection_fraction(
            Point(-10, 0), Point(10, 0), Point(0, -5), Point(0, 5)
        )
        self.assertAlmostEqual(u, 0.5)

    def test_no_crossing(self):
        u = segment_intersection_fraction(
            Point(-10, 10), Point(10, 10), Point(0, -5), Point(0, 5)
        )
        self.assertIsNone(u)

    def test_endpoint_crossing(self):
        u = segment_intersection_fraction(
            Point(-10, -5), Point(0, -5), Point(0, -5), Point(0, 5)
        )
        self.assertAlmostEqual(u, 1.0)

    def test_parallel_is_not_crossing(self):
        u = segment_intersection_fraction(
            Point(-10, 1), Point(10, 1), Point(-5, 1), Point(5, 1)
        )
        self.assertIsNone(u)

    def test_time_interpolation(self):
        self.assertAlmostEqual(interpolate_time(17.320, 17.360, 0.62), 17.3448)

    def test_time_interpolation_rejects_bad_fraction(self):
        with self.assertRaises(ValueError):
            interpolate_time(1.0, 2.0, 1.1)


if __name__ == "__main__":
    unittest.main()
