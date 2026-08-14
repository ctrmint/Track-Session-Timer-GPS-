#!/usr/bin/env python3
"""Create, refine, validate, import, and export versioned track definitions."""

from __future__ import annotations

import argparse
from copy import deepcopy
import json
import math
from pathlib import Path
import sys
import tempfile

from jsonschema import Draft202012Validator, FormatChecker

try:
    from tools.validate_tracks import validate_track_geometry
except ModuleNotFoundError:  # Direct `python tools/track_workbench.py` execution.
    from validate_tracks import validate_track_geometry


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SCHEMA_PATH = REPO_ROOT / "data" / "tracks" / "schema.json"
GATE_NAMES = ("start", "finish", "pit_entry", "pit_exit")
WGS84_SEMI_MAJOR_AXIS_M = 6_378_137.0
WGS84_FLATTENING = 1.0 / 298.257223563
WGS84_ECCENTRICITY_SQUARED = WGS84_FLATTENING * (2.0 - WGS84_FLATTENING)


class TrackWorkbenchError(ValueError):
    """Raised when a definition cannot safely enter or leave the workbench."""


def _meters_per_radian(latitude_deg: float) -> tuple[float, float]:
    latitude_rad = math.radians(latitude_deg)
    sine_latitude = math.sin(latitude_rad)
    denominator = math.sqrt(
        1.0 - WGS84_ECCENTRICITY_SQUARED * sine_latitude * sine_latitude
    )
    prime_vertical_radius = WGS84_SEMI_MAJOR_AXIS_M / denominator
    meridional_radius = (
        WGS84_SEMI_MAJOR_AXIS_M
        * (1.0 - WGS84_ECCENTRICITY_SQUARED)
        / denominator**3
    )
    return (
        prime_vertical_radius * math.cos(latitude_rad),
        meridional_radius,
    )


def _point_from_offset(center: dict, east_m: float, north_m: float) -> dict:
    east_per_radian, north_per_radian = _meters_per_radian(center["lat_deg"])
    longitude = center["lon_deg"] + math.degrees(east_m / east_per_radian)
    if longitude > 180.0:
        longitude -= 360.0
    elif longitude < -180.0:
        longitude += 360.0
    return {
        "lat_deg": center["lat_deg"] + math.degrees(north_m / north_per_radian),
        "lon_deg": longitude,
    }


def derive_gate(
    center_lat_deg: float,
    center_lon_deg: float,
    expected_heading_deg: float,
    width_m: float,
    *,
    heading_tolerance_deg: float = 60.0,
    minimum_crossing_speed_mps: float = 1.0,
    rearm_corridor_m: float = 10.0,
) -> dict:
    """Derive left/right endpoints perpendicular to the expected crossing heading."""
    values = (
        center_lat_deg,
        center_lon_deg,
        expected_heading_deg,
        width_m,
        heading_tolerance_deg,
        minimum_crossing_speed_mps,
        rearm_corridor_m,
    )
    if not all(math.isfinite(value) for value in values):
        raise TrackWorkbenchError("gate capture values must be finite")
    if not -85.0 <= center_lat_deg <= 85.0 or not -180.0 <= center_lon_deg <= 180.0:
        raise TrackWorkbenchError("gate centre is outside the supported coordinate range")
    if not 0.0 <= expected_heading_deg < 360.0:
        raise TrackWorkbenchError("expected heading must be in [0, 360)")
    if not 1.0 <= width_m <= 1_000.0:
        raise TrackWorkbenchError("gate width must be between 1 and 1000 metres")

    heading_rad = math.radians(expected_heading_deg)
    half_width_m = width_m / 2.0
    left_east_m = -math.cos(heading_rad) * half_width_m
    left_north_m = math.sin(heading_rad) * half_width_m
    center = {"lat_deg": center_lat_deg, "lon_deg": center_lon_deg}
    return {
        "left": _point_from_offset(center, left_east_m, left_north_m),
        "right": _point_from_offset(center, -left_east_m, -left_north_m),
        "direction_heading_deg": expected_heading_deg,
        "heading_tolerance_deg": heading_tolerance_deg,
        "minimum_crossing_speed_mps": minimum_crossing_speed_mps,
        "rearm_corridor_m": rearm_corridor_m,
    }


def load_schema(path: Path = DEFAULT_SCHEMA_PATH) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def validate_definition(instance: dict, schema: dict | None = None) -> list[str]:
    """Return schema and semantic failures using stable field paths."""
    selected_schema = schema if schema is not None else load_schema()
    validator = Draft202012Validator(
        selected_schema, format_checker=FormatChecker()
    )
    schema_errors = sorted(
        validator.iter_errors(instance), key=lambda error: list(error.absolute_path)
    )
    failures = [
        f"{'.'.join(str(part) for part in error.absolute_path) or '<root>'}: {error.message}"
        for error in schema_errors
    ]
    if not failures:
        failures.extend(validate_track_geometry(instance))
    return failures


def require_valid(instance: dict, schema: dict | None = None) -> None:
    failures = validate_definition(instance, schema)
    if failures:
        raise TrackWorkbenchError("\n".join(failures))


def read_definition(path: Path, schema: dict | None = None) -> dict:
    try:
        instance = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise TrackWorkbenchError(f"{path}: {error}") from error
    require_valid(instance, schema)
    return instance


def write_definition(path: Path, instance: dict, schema: dict | None = None) -> None:
    """Validate first, then atomically replace the destination file."""
    require_valid(instance, schema)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", dir=path.parent, prefix=f".{path.name}.",
            suffix=".tmp", delete=False
        ) as temporary:
            json.dump(instance, temporary, indent=2)
            temporary.write("\n")
            temporary.flush()
            temporary_path = Path(temporary.name)
        temporary_path.replace(path)
    finally:
        if temporary_path is not None and temporary_path.exists():
            temporary_path.unlink()


def create_definition(
    *,
    track_id: str,
    name: str,
    country: str,
    source: str,
    license_name: str,
    verified_utc: str,
    reference_lat_deg: float,
    reference_lon_deg: float,
    geofence_radius_m: float,
    gate_specs: dict[str, tuple[float, float, float, float]],
    minimum_lap_time_s: float,
) -> dict:
    if set(gate_specs) != set(GATE_NAMES):
        missing = sorted(set(GATE_NAMES) - set(gate_specs))
        extra = sorted(set(gate_specs) - set(GATE_NAMES))
        raise TrackWorkbenchError(f"gate set mismatch; missing={missing}, extra={extra}")
    gates = {}
    for gate_name in GATE_NAMES:
        latitude, longitude, heading, width = gate_specs[gate_name]
        is_pit_gate = gate_name in ("pit_entry", "pit_exit")
        gates[gate_name] = derive_gate(
            latitude,
            longitude,
            heading,
            width,
            minimum_crossing_speed_mps=1.0 if is_pit_gate else 2.0,
            rearm_corridor_m=10.0 if is_pit_gate else 15.0,
        )
    instance = {
        "schema_version": 2,
        "revision": 1,
        "track_id": track_id,
        "name": name,
        "country": country,
        "provenance": {
            "source": source,
            "license": license_name,
            "verified_utc": verified_utc,
        },
        "reference": {
            "lat_deg": reference_lat_deg,
            "lon_deg": reference_lon_deg,
        },
        "geofence": {
            "center_lat_deg": reference_lat_deg,
            "center_lon_deg": reference_lon_deg,
            "radius_m": geofence_radius_m,
        },
        "gates": gates,
        "timing": {"minimum_lap_time_s": minimum_lap_time_s},
        "sectors": [],
    }
    require_valid(instance)
    return instance


def refine_gate(
    instance: dict,
    gate_name: str,
    center_lat_deg: float,
    center_lon_deg: float,
    expected_heading_deg: float,
    width_m: float,
    verified_utc: str,
    *,
    source: str | None = None,
    license_name: str | None = None,
) -> dict:
    if gate_name not in GATE_NAMES:
        raise TrackWorkbenchError(f"unknown gate {gate_name!r}")
    require_valid(instance)
    revised = deepcopy(instance)
    prior = revised["gates"][gate_name]
    revised["gates"][gate_name] = derive_gate(
        center_lat_deg,
        center_lon_deg,
        expected_heading_deg,
        width_m,
        heading_tolerance_deg=prior["heading_tolerance_deg"],
        minimum_crossing_speed_mps=prior["minimum_crossing_speed_mps"],
        rearm_corridor_m=prior["rearm_corridor_m"],
    )
    revised["revision"] += 1
    revised["provenance"]["verified_utc"] = verified_utc
    if source is not None:
        revised["provenance"]["source"] = source
    if license_name is not None:
        revised["provenance"]["license"] = license_name
    require_valid(revised)
    return revised


def import_definition(source: Path, database_dir: Path, *, replace: bool = False) -> Path:
    instance = read_definition(source)
    destination = database_dir / f"{instance['track_id']}.json"
    if destination.exists():
        if not replace:
            raise TrackWorkbenchError(f"{destination}: already exists; use --replace")
        current = read_definition(destination)
        if instance["track_id"] != current["track_id"]:
            raise TrackWorkbenchError("replacement track identifier does not match")
        if instance["revision"] <= current["revision"]:
            raise TrackWorkbenchError("replacement revision must advance")
    write_definition(destination, instance)
    return destination


def export_definition(database_dir: Path, track_id: str, destination: Path) -> Path:
    instance = read_definition(database_dir / f"{track_id}.json")
    write_definition(destination, instance)
    return destination


def _parse_gate_spec(value: str) -> tuple[str, tuple[float, float, float, float]]:
    parts = value.split(",")
    if len(parts) != 5 or parts[0] not in GATE_NAMES:
        raise argparse.ArgumentTypeError(
            "gate must be NAME,LAT,LON,HEADING,WIDTH with a required gate name"
        )
    try:
        return parts[0], tuple(float(part) for part in parts[1:])
    except ValueError as error:
        raise argparse.ArgumentTypeError("gate coordinates and geometry must be numeric") from error


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)

    validate = commands.add_parser("validate", help="validate one definition")
    validate.add_argument("definition", type=Path)

    new = commands.add_parser("new", help="create a validated definition from four captures")
    new.add_argument("--track-id", required=True)
    new.add_argument("--name", required=True)
    new.add_argument("--country", required=True)
    new.add_argument("--source", required=True)
    new.add_argument("--license", dest="license_name", required=True)
    new.add_argument("--verified-utc", required=True)
    new.add_argument("--reference-lat", type=float, required=True)
    new.add_argument("--reference-lon", type=float, required=True)
    new.add_argument("--geofence-radius-m", type=float, default=5_000.0)
    new.add_argument("--minimum-lap-time-s", type=float, default=20.0)
    new.add_argument("--gate", action="append", type=_parse_gate_spec, required=True)
    new.add_argument("--output", type=Path, required=True)

    refine = commands.add_parser("refine", help="replace one gate and advance revision")
    refine.add_argument("definition", type=Path)
    refine.add_argument("gate", choices=GATE_NAMES)
    refine.add_argument("--center-lat", type=float, required=True)
    refine.add_argument("--center-lon", type=float, required=True)
    refine.add_argument("--heading", type=float, required=True)
    refine.add_argument("--width-m", type=float, required=True)
    refine.add_argument("--verified-utc", required=True)
    refine.add_argument("--source")
    refine.add_argument("--license", dest="license_name")
    refine.add_argument("--output", type=Path, required=True)

    import_parser = commands.add_parser("import", help="validate and install in a database")
    import_parser.add_argument("definition", type=Path)
    import_parser.add_argument("--database-dir", type=Path, required=True)
    import_parser.add_argument("--replace", action="store_true")

    export_parser = commands.add_parser("export", help="validate and export from a database")
    export_parser.add_argument("track_id")
    export_parser.add_argument("--database-dir", type=Path, required=True)
    export_parser.add_argument("--output", type=Path, required=True)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = _build_parser().parse_args(argv)
    try:
        if args.command == "validate":
            read_definition(args.definition)
            print(f"Valid track definition: {args.definition}")
        elif args.command == "new":
            gate_specs = dict(args.gate)
            if len(args.gate) != len(gate_specs):
                raise TrackWorkbenchError("each of the four gate names must appear exactly once")
            definition = create_definition(
                track_id=args.track_id,
                name=args.name,
                country=args.country,
                source=args.source,
                license_name=args.license_name,
                verified_utc=args.verified_utc,
                reference_lat_deg=args.reference_lat,
                reference_lon_deg=args.reference_lon,
                geofence_radius_m=args.geofence_radius_m,
                gate_specs=gate_specs,
                minimum_lap_time_s=args.minimum_lap_time_s,
            )
            write_definition(args.output, definition)
            print(f"Created revision 1: {args.output}")
        elif args.command == "refine":
            current = read_definition(args.definition)
            revised = refine_gate(
                current,
                args.gate,
                args.center_lat,
                args.center_lon,
                args.heading,
                args.width_m,
                args.verified_utc,
                source=args.source,
                license_name=args.license_name,
            )
            write_definition(args.output, revised)
            print(f"Created revision {revised['revision']}: {args.output}")
        elif args.command == "import":
            print(import_definition(args.definition, args.database_dir, replace=args.replace))
        elif args.command == "export":
            print(export_definition(args.database_dir, args.track_id, args.output))
    except TrackWorkbenchError as error:
        print(error, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
