#!/usr/bin/env python3
"""Validate the track schema and every committed track definition."""

from __future__ import annotations

import json
import math
from pathlib import Path

from jsonschema import Draft202012Validator


EARTH_RADIUS_M = 6_371_000.0
REQUIRED_GATES = ("start", "finish", "pit_entry", "pit_exit")


def validate_track_geometry(instance: dict) -> list[str]:
    """Return semantic geometry failures not expressible in JSON Schema."""
    failures: list[str] = []
    for gate_name in REQUIRED_GATES:
        gate = instance["gates"][gate_name]
        left = gate["left"]
        right = gate["right"]
        midpoint_latitude_rad = math.radians(
            (left["lat_deg"] + right["lat_deg"]) / 2.0
        )
        east_m = (
            math.radians(right["lon_deg"] - left["lon_deg"])
            * EARTH_RADIUS_M
            * math.cos(midpoint_latitude_rad)
        )
        north_m = (
            math.radians(right["lat_deg"] - left["lat_deg"])
            * EARTH_RADIUS_M
        )
        if math.hypot(east_m, north_m) < 1.0:
            failures.append(
                f"gates.{gate_name}: directed gate is shorter than 1 metre"
            )
    return failures


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    tracks_dir = repo_root / "data" / "tracks"
    schema_path = tracks_dir / "schema.json"
    schema = json.loads(schema_path.read_text(encoding="utf-8"))

    Draft202012Validator.check_schema(schema)
    validator = Draft202012Validator(schema)
    failures: list[str] = []
    track_paths = [path for path in sorted(tracks_dir.glob("*.json")) if path != schema_path]

    for track_path in track_paths:
        instance = json.loads(track_path.read_text(encoding="utf-8"))
        schema_errors = sorted(
            validator.iter_errors(instance), key=lambda item: list(item.path)
        )
        for error in schema_errors:
            location = ".".join(str(part) for part in error.absolute_path) or "<root>"
            failures.append(f"{track_path.relative_to(repo_root)}:{location}: {error.message}")
        if not schema_errors:
            failures.extend(
                f"{track_path.relative_to(repo_root)}:{failure}"
                for failure in validate_track_geometry(instance)
            )

    if failures:
        print("\n".join(failures))
        return 1

    print(f"Validated track schema and {len(track_paths)} track definition(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
