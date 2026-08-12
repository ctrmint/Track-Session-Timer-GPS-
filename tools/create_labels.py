#!/usr/bin/env python3
"""Preview or create the repository's standard GitHub labels."""

from __future__ import annotations
import argparse
import csv
import subprocess
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--execute", action="store_true")
    args = parser.parse_args()
    repo_root = Path(__file__).resolve().parents[1]
    with (repo_root / "planning" / "labels.csv").open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    for row in rows:
        cmd = [
            "gh", "label", "create", row["Name"],
            "--color", row["Color"],
            "--description", row["Description"],
            "--force",
        ]
        print(" ".join(repr(x) if " " in x else x for x in cmd))
        if args.execute:
            subprocess.run(cmd, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
