import hashlib
import json
from copy import deepcopy
from pathlib import Path
import tempfile
import unittest
import zipfile

from tools.build_uk_track_pack import (
    PROMOTED_STATUSES,
    DEFAULT_MANIFEST,
    TrackPackError,
    build_pack,
    validate_manifest,
)
from tools.derive_osm_gates import derive
from tools.track_workbench import validate_definition


class UkTrackPackTests(unittest.TestCase):
    def test_pack_is_complete_valid_and_deterministic(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "uk"
            first = build_pack(DEFAULT_MANIFEST, output)
            self.assertEqual(first["layout_count"], 35)
            self.assertEqual(first["definition_count"], 24)
            self.assertEqual(first["blocked_count"], 11)
            # Donington's two layouts share one validated gate profile. If this ever
            # reaches zero, every circuit in the pack is timer-only again and the device
            # cannot arm anywhere real - which is the state #139 existed to end.
            self.assertEqual(first["timing_ready_count"], 2)
            package_manifest = json.loads(
                (output / "pack-manifest.json").read_text(encoding="utf-8")
            )
            self.assertEqual(
                {layout["nation"] for layout in package_manifest["layouts"]},
                {"England", "Scotland", "Wales", "Northern Ireland"},
            )
            for layout in package_manifest["layouts"]:
                if layout["geometry_status"] == "blocked":
                    self.assertFalse(layout["timing_ready"])
                    self.assertTrue(layout["blocker"])
                    continue
                definition_path = output / layout["definition"]
                definition = json.loads(definition_path.read_text(encoding="utf-8"))
                status = definition["provenance"]["geometry_status"]
                # The packaged summary and the definition itself must agree about whether
                # a track can be timed. A driver reads one and the engine reads the other.
                self.assertEqual(status, layout["geometry_status"])
                self.assertEqual(layout["timing_ready"], status in PROMOTED_STATUSES)
                if status != "provisional":
                    self.assertIn("validated", definition["provenance"]["source"])
                self.assertEqual(validate_definition(definition), [])
                self.assertEqual(
                    hashlib.sha256(definition_path.read_bytes()).hexdigest(),
                    layout["sha256"],
                )
            archive = Path(first["archive"])
            first_hash = hashlib.sha256(archive.read_bytes()).hexdigest()
            with zipfile.ZipFile(archive) as packaged:
                self.assertIn("pack-manifest.json", packaged.namelist())
                self.assertEqual(
                    len([name for name in packaged.namelist() if name.startswith("definitions/")]),
                    24,
                )
            second = build_pack(DEFAULT_MANIFEST, output)
            self.assertEqual(
                hashlib.sha256(Path(second["archive"]).read_bytes()).hexdigest(),
                first_hash,
            )

    def test_manifest_requires_all_four_nations_and_explicit_blockers(self):
        manifest = json.loads(DEFAULT_MANIFEST.read_text(encoding="utf-8"))
        missing_nation = deepcopy(manifest)
        missing_nation["venues"] = [
            venue
            for venue in missing_nation["venues"]
            if venue["nation"] != "Northern Ireland"
        ]
        with self.assertRaisesRegex(TrackPackError, "all four UK nations"):
            validate_manifest(missing_nation)

        unsafe_layout = deepcopy(manifest)
        unsafe_layout["venues"][0]["layouts"][0].pop("blocker")
        with self.assertRaisesRegex(TrackPackError, "exactly one"):
            validate_manifest(unsafe_layout)

    def test_a_promotion_without_real_corroboration_is_refused(self):
        """The gate that stops unverified geometry becoming a believable lap time.

        Every case here is a plausible shortcut someone could take in good faith, and each
        would produce a pack that looks validated and is not.
        """
        manifest = json.loads(DEFAULT_MANIFEST.read_text(encoding="utf-8"))

        def build_with(validation):
            candidate = deepcopy(manifest)
            venue = next(
                v for v in candidate["venues"] if v["venue_id"] == "donington_park"
            )
            if validation is None:
                venue["gate_profiles"]["default"].pop("validation", None)
            else:
                venue["gate_profiles"]["default"]["validation"] = validation
            with tempfile.TemporaryDirectory() as temporary:
                path = Path(temporary) / "manifest.json"
                path.write_text(json.dumps(candidate), encoding="utf-8")
                return build_pack(path, Path(temporary) / "uk")

        good = deepcopy(
            next(
                v for v in manifest["venues"] if v["venue_id"] == "donington_park"
            )["gate_profiles"]["default"]["validation"]
        )

        # One source is not corroboration, however good it is.
        single = deepcopy(good)
        single["sources"] = single["sources"][:1]
        with self.assertRaisesRegex(TrackPackError, "at least two sources"):
            build_with(single)

        # Two readings of the same database are one source read twice.
        same_origin = deepcopy(good)
        same_origin["sources"][1]["origin"] = same_origin["sources"][0]["origin"]
        with self.assertRaisesRegex(TrackPackError, "two distinct origins"):
            build_with(same_origin)

        # Sources that disagree by more than the threshold have not corroborated anything;
        # they have identified a discrepancy nobody resolved.
        far_apart = deepcopy(good)
        far_apart["agreement_m"] = 25.0
        with self.assertRaisesRegex(TrackPackError, "exceeds the"):
            build_with(far_apart)

        # A status outside the ladder cannot be invented.
        invented = deepcopy(good)
        invented["geometry_status"] = "looks_about_right"
        with self.assertRaisesRegex(TrackPackError, "geometry_status must be"):
            build_with(invented)

        # Evidence without a date cannot be reviewed against a pack revision later.
        undated = deepcopy(good)
        undated.pop("verified_utc")
        with self.assertRaisesRegex(TrackPackError, "verified_utc"):
            build_with(undated)

        # And with no validation record at all, the definition stays provisional rather
        # than inheriting anything from its neighbours.
        result = build_with(None)
        self.assertEqual(result["timing_ready_count"], 0)

    def test_osm_derivation_never_claims_timing_readiness(self):
        snapshot = {
            "elements": [
                {
                    "id": 10,
                    "geometry": [
                        {"lat": 52.0, "lon": -1.001},
                        {"lat": 52.0, "lon": -1.0},
                        {"lat": 52.0, "lon": -0.999},
                    ],
                    "tags": {"highway": "raceway", "oneway": "yes", "sport": "motor"},
                },
                {
                    "id": 20,
                    "geometry": [
                        {"lat": 52.0001, "lon": -1.001},
                        {"lat": 52.0001, "lon": -1.0},
                        {"lat": 52.0001, "lon": -0.999},
                    ],
                    "tags": {
                        "highway": "raceway",
                        "name": "Pit Lane",
                        "oneway": "yes",
                        "sport": "motor",
                    },
                },
            ]
        }
        result = derive(snapshot, 20)
        self.assertEqual(result["geometry_status"], "provisional")
        self.assertEqual(result["source_way_ids"], {"main": 10, "pit": 20})
        self.assertAlmostEqual(result["gates"]["start"]["heading_deg"], 90.0)


if __name__ == "__main__":
    unittest.main()
