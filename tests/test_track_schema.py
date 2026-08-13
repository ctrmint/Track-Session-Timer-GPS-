import json
from copy import deepcopy
import unittest
from pathlib import Path

from jsonschema import Draft202012Validator


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


if __name__ == "__main__":
    unittest.main()
