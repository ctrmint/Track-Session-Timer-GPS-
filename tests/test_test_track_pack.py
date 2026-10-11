import json
import math
from copy import deepcopy
from pathlib import Path
import tempfile
import unittest

from tools.build_track_pack import TrackPackError, build_pack, validate_manifest
from tools.track_workbench import validate_definition

REPO_ROOT = Path(__file__).resolve().parents[1]
TEST_MANIFEST = REPO_ROOT / "data" / "track-packs" / "test" / "manifest.json"

EARTH_RADIUS_M = 6_371_008.8

# The loop as supplied in #184, clockwise. Held here independently of the manifest so the
# tests check the manifest against the agreed geometry rather than against itself.
LOOP = [
    (54.169676, -4.446770),
    (54.169142, -4.446392),
    (54.168699, -4.446843),
    (54.167802, -4.447591),
    (54.166978, -4.448040),
    (54.166475, -4.449074),
    (54.166359, -4.449993),
    (54.166441, -4.450427),
    (54.166558, -4.450468),
    (54.166707, -4.450342),
    (54.167172, -4.449786),
    (54.168049, -4.448762),
    (54.169208, -4.447405),
    (54.169676, -4.446770),
]
ROAD_WIDTH_M = 25 * 0.3048


def _to_local(latitude, longitude, origin):
    east_per_degree = math.radians(1.0) * EARTH_RADIUS_M * math.cos(math.radians(origin[0]))
    north_per_degree = math.radians(1.0) * EARTH_RADIUS_M
    return ((longitude - origin[1]) * east_per_degree, (latitude - origin[0]) * north_per_degree)


def _distance_to_segment(point, start, end):
    east, north = end[0] - start[0], end[1] - start[1]
    length_squared = east * east + north * north
    along = 0.0
    if length_squared > 0:
        along = ((point[0] - start[0]) * east + (point[1] - start[1]) * north) / length_squared
        along = max(0.0, min(1.0, along))
    return math.hypot(point[0] - (start[0] + along * east), point[1] - (start[1] + along * north))


def _gate_midpoint_and_width(gate):
    mean_latitude = (gate["left"]["lat_deg"] + gate["right"]["lat_deg"]) / 2.0
    east_per_degree = math.radians(1.0) * EARTH_RADIUS_M * math.cos(math.radians(mean_latitude))
    north_per_degree = math.radians(1.0) * EARTH_RADIUS_M
    width = math.hypot(
        (gate["right"]["lon_deg"] - gate["left"]["lon_deg"]) * east_per_degree,
        (gate["right"]["lat_deg"] - gate["left"]["lat_deg"]) * north_per_degree,
    )
    return (mean_latitude, (gate["left"]["lon_deg"] + gate["right"]["lon_deg"]) / 2.0), width


class TestTrackPackTests(unittest.TestCase):
    def setUp(self):
        self.manifest = json.loads(TEST_MANIFEST.read_text(encoding="utf-8"))
        self.profile = self.manifest["venues"][0]["gate_profiles"]["default"]

    def _build(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "test"
            result = build_pack(TEST_MANIFEST, output)
            definition = json.loads(
                (output / "definitions" / "im_test_circuit.json").read_text(encoding="utf-8")
            )
            return result, definition

    def test_pack_builds_one_definition_that_the_schema_accepts(self):
        result, definition = self._build()
        self.assertEqual(result["definition_count"], 1)
        self.assertEqual(result["blocked_count"], 0)
        validate_definition(definition)
        self.assertEqual(definition["track_id"], "im_test_circuit")
        self.assertEqual(definition["name"], "Test Circuit")
        # The Isle of Man is a Crown Dependency, not part of the United Kingdom, which is
        # why this venue cannot live in the UK pack at all.
        self.assertEqual(definition["country"], "IM")
        self.assertEqual(definition["provenance"]["license"], "ODbL-1.0")

    def test_the_fixture_cannot_arm_timing_until_it_is_captured(self):
        """A road loop has no route to independently_validated, so it must not claim one.

        There is no pit lane to identify a start/finish straight from and no second
        independent source for a line somebody chose on a road, so the only honest
        promotion is a device capture. Until then this must stay timer-only, or the
        provenance gate #139 built has been holed to accommodate a test fixture.
        """
        result, definition = self._build()
        self.assertEqual(result["timing_ready_count"], 0)
        self.assertEqual(definition["provenance"]["geometry_status"], "provisional")
        self.assertNotIn("validation", self.profile)

    def test_every_gate_spans_the_road_with_margin_but_not_the_other_carriageway(self):
        """12 m is the whole of the width decision, and both bounds matter.

        Too narrow and a car near the kerb on a mediocre fix misses the lap, because it is
        the reported position that crosses the line rather than the vehicle. Too wide and
        the gate reaches onto the opposite carriageway, where the legs come within 13.3 m
        of each other at the Southern Junction.
        """
        _, definition = self._build()
        for name, gate in definition["gates"].items():
            _, width = _gate_midpoint_and_width(gate)
            self.assertGreater(width, ROAD_WIDTH_M, f"{name} is narrower than the road")
            self.assertLess(width, 19.0, f"{name} could reach the opposite carriageway")

    def test_the_start_line_is_clear_of_the_outbound_leg(self):
        """The supplied Start and Finish are the same point, where both legs meet.

        A gate there would span the departing carriageway as well as the arriving one, so
        the line is set back along King Edward Road. This checks the set-back actually
        bought clearance rather than being asserted in a comment.
        """
        _, definition = self._build()
        origin = LOOP[0]
        start, width = _gate_midpoint_and_width(definition["gates"]["start"])
        local = _to_local(start[0], start[1], origin)
        loop = [_to_local(latitude, longitude, origin) for latitude, longitude in LOOP]

        # Clearance from the outbound leg, which leaves the junction on Sea Cliff Road.
        outbound = min(_distance_to_segment(local, loop[i], loop[i + 1]) for i in range(2))
        self.assertGreater(outbound, 20.0)
        # The gate end must stop short of the other carriageway, not merely of its centre.
        self.assertGreater(outbound - width / 2.0 - ROAD_WIDTH_M / 2.0, 5.0)
        # And it must still be on the return leg rather than somewhere off the circuit.
        on_return = min(_distance_to_segment(local, loop[i], loop[i + 1]) for i in range(11, 13))
        self.assertLess(on_return, 5.0)

    def test_the_pit_exit_faces_the_way_the_car_leaves_the_pit_lane(self):
        """Both pit gates are on Harbour Road, so pit_exit is passed twice per stop.

        Once north-bound on the way in, once south-bound on the way out. Only the heading
        separates them, so a reversed heading would register every exit on the way in and
        miss it on the way out.
        """
        _, definition = self._build()
        heading = definition["gates"]["pit_exit"]["direction_heading_deg"]
        self.assertAlmostEqual(heading, 147.3, places=1)
        tolerance = definition["gates"]["pit_exit"]["heading_tolerance_deg"]
        # The two passes are 180 degrees apart, so the tolerance must not span both.
        self.assertLess(tolerance, 90.0)

    def test_the_pack_cannot_quietly_become_a_uk_circuit_pack(self):
        """The UK pack's rules are its own claims, and this pack must not inherit them.

        It declares no required nations because it asserts no national coverage, and it
        cites no official source because a public road has no operator to cite. Both are
        absences on purpose, so each is checked rather than assumed.
        """
        self.assertNotIn("required_nations", self.manifest)
        self.assertNotIn("official_source", self.manifest["venues"][0])
        self.assertEqual(self.manifest["pack_id"], "test-circuits")

        # An invented citation would be worse than no citation, but a malformed one that
        # is present must still be refused.
        invalid = deepcopy(self.manifest)
        invalid["venues"][0]["official_source"] = "the road outside"
        with self.assertRaisesRegex(TrackPackError, "official_source must be an HTTPS source"):
            validate_manifest(invalid)

    def test_the_supplied_loop_is_recorded_so_the_gates_can_be_rechecked(self):
        """The definition schema has no field for a route, so the points live here.

        Without them the 14 coordinates the gates were derived from would exist only in
        the issue thread, and nobody could re-derive the start line.
        """
        points = self.manifest["venues"][0]["source_points"]
        self.assertEqual(len(points), len(LOOP))
        for recorded, (latitude, longitude) in zip(points, LOOP):
            self.assertAlmostEqual(recorded["lat"], latitude, places=6)
            self.assertAlmostEqual(recorded["lon"], longitude, places=6)
        self.assertEqual(points[0]["name"], "Start")
        self.assertEqual(points[-1]["name"], "Finish")
        # Named in #184 as a navigational marker only, so it must not have become a gate.
        self.assertIn("Southern Junction", [point["name"] for point in points])
        _, definition = self._build()
        self.assertEqual(definition["sectors"], [])

    def test_a_lap_cannot_be_shorter_than_the_road_allows(self):
        """943 m at 60 mph is 35 s, and both named roads are limited to 30 mph.

        The guard exists to reject a double trigger, so it has to sit below any lap that
        could really be driven and above zero.
        """
        _, definition = self._build()
        minimum = definition["timing"]["minimum_lap_time_s"]
        self.assertGreater(minimum, 0.0)
        self.assertLess(minimum, 35.0)


if __name__ == "__main__":
    unittest.main()
