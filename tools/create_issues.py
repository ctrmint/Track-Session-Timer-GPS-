#!/usr/bin/env python3
"""Preview or create GitHub issues from planning/issues.csv.

Run from the repository root after `gh auth login`.
The default mode only prints commands. Pass --execute to create issues.
"""

from __future__ import annotations
import argparse
import csv
import json
import shlex
import subprocess
from pathlib import Path


def existing_issue_titles(repo: str | None) -> set[str]:
    cmd = ["gh", "issue", "list", "--state", "all", "--limit", "1000", "--json", "title"]
    if repo:
        cmd += ["--repo", repo]
    result = subprocess.run(cmd, check=True, capture_output=True, text=True)
    return {item["title"] for item in json.loads(result.stdout)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--execute", action="store_true", help="actually run gh issue create")
    parser.add_argument("--repo", help="target repository in OWNER/REPO form")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    issues_file = repo_root / "planning" / "issues.csv"

    with issues_file.open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    existing_titles = existing_issue_titles(args.repo) if args.execute else set()
    for row in rows:
        title = row["Title"]
        body = (
            f"Planning snapshot ID: {row['ID']}\n\n"
            f"Depends on: {row['DependsOn'] or 'none'}\n\n"
            f"## Acceptance criteria\n\n- {row['AcceptanceSummary']}\n"
        )
        cmd = ["gh", "issue", "create", "--title", title, "--body", body]
        if args.repo:
            cmd += ["--repo", args.repo]
        if row["Milestone"]:
            cmd += ["--milestone", row["Milestone"]]
        for label in [x.strip() for x in row["Labels"].split(",") if x.strip()]:
            cmd += ["--label", label]

        print(shlex.join(cmd))
        if args.execute:
            if title in existing_titles:
                print(f"skip existing issue: {title}")
                continue
            subprocess.run(cmd, check=True)
            existing_titles.add(title)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
