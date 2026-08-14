import hashlib
import json
from copy import deepcopy
from pathlib import Path
import tempfile
import unittest
import zipfile

from tools.build_uk_track_pack import (
    DEFAULT_MANIFEST,
    TrackPackError,
    build_pack,
    validate_manifest,
)
from tools.derive_osm_gates import derive
from tools.track_workbench import validate_definition


class UkTrackPackTests(unittest.TestCase):
    def test_pack_is_complete_provisional_valid_and_deterministic(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "uk"
            first = build_pack(DEFAULT_MANIFEST, output)
            self.assertEqual(first["layout_count"], 35)
            self.assertEqual(first["definition_count"], 24)
            self.assertEqual(first["blocked_count"], 11)
            package_manifest = json.loads(
                (output / "pack-manifest.json").read_text(encoding="utf-8")
            )
            self.assertEqual(
                {layout["nation"] for layout in package_manifest["layouts"]},
                {"England", "Scotland", "Wales", "Northern Ireland"},
            )
            for layout in package_manifest["layouts"]:
                self.assertFalse(layout["timing_ready"])
                if layout["geometry_status"] == "blocked":
                    self.assertTrue(layout["blocker"])
                    continue
                definition_path = output / layout["definition"]
                definition = json.loads(definition_path.read_text(encoding="utf-8"))
                self.assertEqual(
                    definition["provenance"]["geometry_status"], "provisional"
                )
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
