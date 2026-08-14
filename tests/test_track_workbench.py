import json
from pathlib import Path
import tempfile
import unittest

from tools.track_workbench import (
    TrackWorkbenchError,
    create_definition,
    derive_gate,
    export_definition,
    import_definition,
    refine_gate,
    validate_definition,
    write_definition,
)


class TrackWorkbenchTests(unittest.TestCase):
    def gate_specs(self):
        return {
            "start": (52.0, -1.0, 90.0, 20.0),
            "finish": (52.0, -1.0, 90.0, 20.0),
            "pit_entry": (52.0002, -1.0, 90.0, 15.0),
            "pit_exit": (51.9998, -1.0, 90.0, 15.0),
        }

    def definition(self):
        return create_definition(
            track_id="workbench_test",
            name="Workbench Test",
            country="GB",
            source="stationary device capture",
            license_name="CC0-1.0",
            verified_utc="2026-08-14T10:00:00Z",
            reference_lat_deg=52.0,
            reference_lon_deg=-1.0,
            geofence_radius_m=2_000.0,
            gate_specs=self.gate_specs(),
            minimum_lap_time_s=20.0,
        )

    def test_gate_is_perpendicular_and_centered(self):
        gate = derive_gate(52.0, -1.0, 90.0, 20.0)
        self.assertGreater(gate["left"]["lat_deg"], 52.0)
        self.assertLess(gate["right"]["lat_deg"], 52.0)
        self.assertAlmostEqual(
            (gate["left"]["lat_deg"] + gate["right"]["lat_deg"]) / 2.0,
            52.0,
        )
        self.assertEqual(validate_definition(self.definition()), [])

    def test_refine_advances_revision_and_provenance(self):
        revised = refine_gate(
            self.definition(),
            "finish",
            52.0001,
            -1.0,
            95.0,
            22.0,
            "2026-08-14T11:00:00Z",
            source="host-refined logged trace",
        )
        self.assertEqual(revised["revision"], 2)
        self.assertEqual(revised["gates"]["finish"]["direction_heading_deg"], 95.0)
        self.assertEqual(revised["provenance"]["source"], "host-refined logged trace")
        self.assertEqual(revised["provenance"]["geometry_status"], "device_captured")
        self.assertEqual(validate_definition(revised), [])

        provisional = self.definition()
        provisional["provenance"]["geometry_status"] = "provisional"
        still_provisional = refine_gate(
            provisional,
            "finish",
            52.0001,
            -1.0,
            95.0,
            22.0,
            "2026-08-14T11:30:00Z",
        )
        self.assertEqual(
            still_provisional["provenance"]["geometry_status"], "provisional"
        )

    def test_import_export_are_validated_and_atomic(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "candidate.json"
            database = root / "database"
            exported = root / "export" / "track.json"
            write_definition(source, self.definition())
            installed = import_definition(source, database)
            self.assertEqual(installed.name, "workbench_test.json")
            export_definition(database, "workbench_test", exported)
            self.assertEqual(
                json.loads(installed.read_text(encoding="utf-8")),
                json.loads(exported.read_text(encoding="utf-8")),
            )

            with self.assertRaisesRegex(TrackWorkbenchError, "already exists"):
                import_definition(source, database)
            with self.assertRaisesRegex(TrackWorkbenchError, "revision must advance"):
                import_definition(source, database, replace=True)

    def test_invalid_geometry_never_imports(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = self.definition()
            candidate["gates"]["pit_entry"]["right"] = candidate["gates"]["pit_entry"][
                "left"
            ]
            source = root / "invalid.json"
            source.write_text(json.dumps(candidate), encoding="utf-8")
            with self.assertRaisesRegex(TrackWorkbenchError, "gates.pit_entry"):
                import_definition(source, root / "database")
            self.assertFalse((root / "database" / "workbench_test.json").exists())


if __name__ == "__main__":
    unittest.main()
