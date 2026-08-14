#!/usr/bin/env python3
"""Validate the track schema and every committed track definition."""

from __future__ import annotations

import json
import math
from pathlib import Path

from jsonschema import Draft202012Validator, FormatChecker


EARTH_RADIUS_M = 6_371_000.0
REQUIRED_GATES = ("start", "finish", "pit_entry", "pit_exit")
MINIMUM_GATE_LENGTH_M = 1.0
MAXIMUM_GATE_LENGTH_M = 1_000.0
MINIMUM_CROSSING_ANGLE_DEG = 10.0


def local_offset_m(origin: dict, point: dict) -> tuple[float, float]:
    midpoint_latitude_rad = math.radians(
        (origin["lat_deg"] + point["lat_deg"]) / 2.0
    )
    return (
        math.radians(point["lon_deg"] - origin["lon_deg"])
        * EARTH_RADIUS_M
        * math.cos(midpoint_latitude_rad),
        math.radians(point["lat_deg"] - origin["lat_deg"]) * EARTH_RADIUS_M,
    )


def gate_entries(instance: dict):
    for gate_name in REQUIRED_GATES:
        yield f"gates.{gate_name}", instance["gates"][gate_name]
    for index, sector in enumerate(instance.get("sectors", [])):
        yield f"sectors.{index}.gate", sector["gate"]


def validate_track_geometry(instance: dict) -> list[str]:
    """Return semantic geometry failures not expressible in JSON Schema."""
    failures: list[str] = []
    geofence = instance["geofence"]
    geofence_center = {
        "lat_deg": geofence["center_lat_deg"],
        "lon_deg": geofence["center_lon_deg"],
    }
    reference_offset = local_offset_m(geofence_center, instance["reference"])
    if math.hypot(*reference_offset) > geofence["radius_m"]:
        failures.append("geofence: reference lies outside the geofence")

    for gate_path, gate in gate_entries(instance):
        left = gate["left"]
        right = gate["right"]
        east_m, north_m = local_offset_m(left, right)
        length_m = math.hypot(east_m, north_m)
        if length_m < MINIMUM_GATE_LENGTH_M:
            failures.append(f"{gate_path}: directed gate is shorter than 1 metre")
            continue
        if length_m > MAXIMUM_GATE_LENGTH_M:
            failures.append(f"{gate_path}: directed gate is longer than 1000 metres")

        heading_rad = math.radians(gate["direction_heading_deg"])
        direction_east = math.sin(heading_rad)
        direction_north = math.cos(heading_rad)
        crossing_sine = abs(
            east_m * direction_north - north_m * direction_east
        ) / length_m
        if crossing_sine < math.sin(math.radians(MINIMUM_CROSSING_ANGLE_DEG)):
            failures.append(
                f"{gate_path}.direction_heading_deg: direction is within 10 degrees of the gate line"
            )

        for endpoint_name, endpoint in (("left", left), ("right", right)):
            if math.hypot(*local_offset_m(geofence_center, endpoint)) > geofence["radius_m"]:
                failures.append(
                    f"{gate_path}.{endpoint_name}: endpoint lies outside the geofence"
                )

    sector_ids: dict[str, int] = {}
    for index, sector in enumerate(instance.get("sectors", [])):
        sector_id = sector["sector_id"]
        if sector_id in sector_ids:
            failures.append(
                f"sectors.{index}.sector_id: duplicates sectors.{sector_ids[sector_id]}.sector_id"
            )
        else:
            sector_ids[sector_id] = index
    return failures


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    tracks_dir = repo_root / "data" / "tracks"
    schema_path = tracks_dir / "schema.json"
    schema = json.loads(schema_path.read_text(encoding="utf-8"))

    Draft202012Validator.check_schema(schema)
    validator = Draft202012Validator(schema, format_checker=FormatChecker())
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
