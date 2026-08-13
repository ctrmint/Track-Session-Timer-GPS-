import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from validate_simulator_fixtures import FIELDS, MARKER, validate_fixture


class SimulatorFixtureTests(unittest.TestCase):
    def test_checked_in_fixture_is_valid(self):
        fixture = (
            Path(__file__).resolve().parents[1]
            / "simulator"
            / "fixtures"
            / "recorded_reference_v1.csv"
        )
        self.assertEqual(validate_fixture(fixture), [])

    def test_non_monotonic_fixture_is_rejected(self):
        header = ",".join(FIELDS)
        rows = [
            "40,52.0,-1.0,100.0,20.0,90.0,0.5,12",
            "40,52.1,-1.1,100.0,20.0,90.0,0.5,12",
        ]
        content = "\n".join([MARKER, "# name=invalid", header, *rows, ""])
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "invalid.csv"
            path.write_text(content, encoding="utf-8")
            failures = validate_fixture(path)
        self.assertTrue(any("not strictly increasing" in failure for failure in failures))

    def test_unknown_version_is_rejected(self):
        fixture = (
            Path(__file__).resolve().parents[1]
            / "simulator"
            / "fixtures"
            / "recorded_reference_v1.csv"
        )
        content = fixture.read_text(encoding="utf-8").replace(MARKER, "# unknown-v2", 1)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "unknown.csv"
            path.write_text(content, encoding="utf-8")
            failures = validate_fixture(path)
        self.assertTrue(any("unsupported version" in failure for failure in failures))


if __name__ == "__main__":
    unittest.main()
