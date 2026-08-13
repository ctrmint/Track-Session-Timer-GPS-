#!/usr/bin/env python3
"""Validate all version 1 GNSS simulator CSV fixtures."""

from __future__ import annotations

import csv
import math
from pathlib import Path


MARKER = "# track-session-timer-gnss-fixture-v1"
NAME_PREFIX = "# name="
FIELDS = [
    "measurement_time_ns",
    "latitude_deg",
    "longitude_deg",
    "height_m",
    "speed_mps",
    "heading_deg",
    "horizontal_accuracy_m",
    "num_satellites",
]


def validate_fixture(path: Path) -> list[str]:
    failures: list[str] = []
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeError) as error:
        return [f"{path}: cannot read fixture: {error}"]

    if len(lines) < 5:
        return [f"{path}: fixture must contain metadata, a header, and at least two rows"]
    if lines[0] != MARKER:
        failures.append(f"{path}: unsupported version marker")
    if not lines[1].startswith(NAME_PREFIX) or not lines[1][len(NAME_PREFIX) :]:
        failures.append(f"{path}: fixture name is missing")

    reader = csv.DictReader(lines[2:])
    if reader.fieldnames != FIELDS:
        failures.append(f"{path}: header does not match version 1")
        return failures

    previous_time: int | None = None
    row_count = 0
    for line_number, row in enumerate(reader, start=4):
        row_count += 1
        if None in row or any(row.get(field) is None for field in FIELDS):
            failures.append(f"{path}:{line_number}: row does not match version 1 header")
            continue
        try:
            measurement_time = int(row["measurement_time_ns"])
            latitude = float(row["latitude_deg"])
            longitude = float(row["longitude_deg"])
            height = float(row["height_m"])
            speed = float(row["speed_mps"])
            heading = float(row["heading_deg"])
            horizontal_accuracy = float(row["horizontal_accuracy_m"])
            satellites = int(row["num_satellites"])
        except (KeyError, TypeError, ValueError):
            failures.append(f"{path}:{line_number}: invalid numeric field")
            continue

        numeric_values = (latitude, longitude, height, speed, heading, horizontal_accuracy)
        if not all(math.isfinite(value) for value in numeric_values):
            failures.append(f"{path}:{line_number}: values must be finite")
        if previous_time is not None and measurement_time <= previous_time:
            failures.append(f"{path}:{line_number}: measurement time is not strictly increasing")
        if not -90.0 <= latitude <= 90.0 or not -180.0 <= longitude <= 180.0:
            failures.append(f"{path}:{line_number}: latitude or longitude is out of range")
        if speed < 0.0 or not 0.0 <= heading < 360.0 or horizontal_accuracy < 0.0:
            failures.append(f"{path}:{line_number}: motion or accuracy value is out of range")
        if not 0 <= satellites <= 64:
            failures.append(f"{path}:{line_number}: satellite count is out of range")
        previous_time = measurement_time

    if row_count < 2:
        failures.append(f"{path}: fixture must contain at least two rows")
    return failures


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    fixtures = sorted((repo_root / "simulator" / "fixtures").glob("*.csv"))
    failures = [failure for path in fixtures for failure in validate_fixture(path)]
    if not fixtures:
        failures.append("simulator/fixtures: no GNSS fixtures found")
    if failures:
        print("\n".join(failures))
        return 1
    print(f"Validated {len(fixtures)} GNSS simulator fixture(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
