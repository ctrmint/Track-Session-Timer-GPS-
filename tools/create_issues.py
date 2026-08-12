#!/usr/bin/env python3
"""Preview or create GitHub issues from planning/issues.csv.

Run from the repository root after `gh auth login`.
The default mode only prints commands. Pass --execute to create issues.
"""

from __future__ import annotations
import argparse
import csv
import subprocess
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--execute", action="store_true", help="actually run gh issue create")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    issues_file = repo_root / "planning" / "issues.csv"

    with issues_file.open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    for row in rows:
        title = f"[{row['ID']}] {row['Title']}"
        body = (
            f"Milestone phase: {row['Milestone']}\n\n"
            f"Depends on planning issue IDs: {row['DependsOn'] or 'none'}\n\n"
            f"## Acceptance criteria\n\n- {row['AcceptanceSummary']}\n"
        )
        cmd = ["gh", "issue", "create", "--title", title, "--body", body]
        for label in [x.strip() for x in row["Labels"].split(",") if x.strip()]:
            cmd += ["--label", label]

        print(" ".join(repr(x) if " " in x or "\n" in x else x for x in cmd))
        if args.execute:
            subprocess.run(cmd, check=True)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
