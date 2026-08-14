import json
from copy import deepcopy
import unittest
from pathlib import Path

from jsonschema import Draft202012Validator, FormatChecker

from tools.validate_tracks import validate_track_geometry


class TrackSchemaTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        tracks_dir = Path(__file__).resolve().parents[1] / "data" / "tracks"
        cls.schema = json.loads((tracks_dir / "schema.json").read_text(encoding="utf-8"))
        cls.example = json.loads(
            (tracks_dir / "synthetic_test_loop.json").read_text(encoding="utf-8")
        )

    def test_schema_is_valid_draft_2020_12(self):
        Draft202012Validator.check_schema(self.schema)

    def test_synthetic_track_matches_schema(self):
        errors = list(Draft202012Validator(self.schema).iter_errors(self.example))
        self.assertEqual(errors, [])

    def test_wrong_track_id_type_is_rejected(self):
        invalid = deepcopy(self.example)
        invalid["track_id"] = 42
        errors = list(Draft202012Validator(self.schema).iter_errors(invalid))
        self.assertTrue(errors)

    def test_all_four_gates_are_required(self):
        for gate_name in ("start", "finish", "pit_entry", "pit_exit"):
            with self.subTest(gate=gate_name):
                invalid = deepcopy(self.example)
                del invalid["gates"][gate_name]
                errors = list(Draft202012Validator(self.schema).iter_errors(invalid))
                self.assertTrue(errors)

    def test_unknown_gate_geometry_is_rejected(self):
        invalid = deepcopy(self.example)
        invalid["gates"]["start"]["box_width_m"] = 12
        errors = list(Draft202012Validator(self.schema).iter_errors(invalid))
        self.assertTrue(errors)

    def test_unknown_root_property_is_rejected_at_root(self):
        invalid = deepcopy(self.example)
        invalid["provenence"] = invalid.pop("provenance")
        errors = list(Draft202012Validator(self.schema).iter_errors(invalid))
        self.assertTrue(any(list(error.absolute_path) == [] for error in errors))

    def test_revision_and_provenance_are_structured(self):
        invalid = deepcopy(self.example)
        invalid["revision"] = 0
        invalid["provenance"]["verified_utc"] = "2026-08-14 00:00:00Z"
        paths = {
            ".".join(str(part) for part in error.absolute_path)
            for error in Draft202012Validator(
                self.schema, format_checker=FormatChecker()
            ).iter_errors(invalid)
        }
        self.assertIn("revision", paths)
        self.assertIn("provenance.verified_utc", paths)

    def test_sector_fields_and_unknown_properties_are_strict(self):
        invalid = deepcopy(self.example)
        invalid["sectors"][0]["sector_id"] = ""
        invalid["sectors"][0]["gate"]["radius_m"] = 5
        paths = {
            ".".join(str(part) for part in error.absolute_path)
            for error in Draft202012Validator(self.schema).iter_errors(invalid)
        }
        self.assertIn("sectors.0.sector_id", paths)
        self.assertIn("sectors.0.gate", paths)

    def test_invalid_gate_limits_are_rejected(self):
        invalid = deepcopy(self.example)
        invalid["gates"]["pit_exit"]["minimum_crossing_speed_mps"] = 0
        errors = list(Draft202012Validator(self.schema).iter_errors(invalid))
        self.assertTrue(errors)

    def test_start_and_finish_may_share_geometry(self):
        self.assertEqual(
            self.example["gates"]["start"], self.example["gates"]["finish"]
        )
        self.assertEqual(validate_track_geometry(self.example), [])

    def test_degenerate_gate_is_rejected_semantically(self):
        invalid = deepcopy(self.example)
        invalid["gates"]["pit_entry"]["right"] = deepcopy(
            invalid["gates"]["pit_entry"]["left"]
        )
        self.assertEqual(
            validate_track_geometry(invalid),
            ["gates.pit_entry: directed gate is shorter than 1 metre"],
        )

    def test_duplicate_sector_identifier_names_exact_field(self):
        invalid = deepcopy(self.example)
        invalid["sectors"].append(deepcopy(invalid["sectors"][0]))
        self.assertEqual(
            validate_track_geometry(invalid),
            ["sectors.1.sector_id: duplicates sectors.0.sector_id"],
        )

    def test_geometry_outside_geofence_names_endpoint(self):
        invalid = deepcopy(self.example)
        invalid["geofence"]["radius_m"] = 10
        failures = validate_track_geometry(invalid)
        self.assertIn(
            "sectors.0.gate.left: endpoint lies outside the geofence", failures
        )


if __name__ == "__main__":
    unittest.main()
