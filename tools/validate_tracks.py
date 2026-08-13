#!/usr/bin/env python3
"""Validate the track schema and every committed track definition."""

from __future__ import annotations

import json
from pathlib import Path

from jsonschema import Draft202012Validator


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
        for error in sorted(validator.iter_errors(instance), key=lambda item: list(item.path)):
            location = ".".join(str(part) for part in error.absolute_path) or "<root>"
            failures.append(f"{track_path.relative_to(repo_root)}:{location}: {error.message}")

    if failures:
        print("\n".join(failures))
        return 1

    print(f"Validated track schema and {len(track_paths)} track definition(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
