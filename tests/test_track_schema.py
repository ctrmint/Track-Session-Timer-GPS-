import json
from copy import deepcopy
import unittest
from pathlib import Path

from jsonschema import Draft202012Validator

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


if __name__ == "__main__":
    unittest.main()
