#!/usr/bin/env python3
"""Check local Markdown links, changelog structure, and tracked generated artefacts."""

from __future__ import annotations

import re
import subprocess
from pathlib import Path
from urllib.parse import unquote, urlsplit

import yaml


LINK_RE = re.compile(r"\[[^\]]+\]\(([^)]+)\)")
IGNORED_PARTS = {".git", ".venv", "build", "managed_components"}

# Keep a Changelog's section types, in the order it specifies. The changelog is edited
# by hand on nearly every change, which makes it one of the easier files to damage
# without noticing: a typo'd heading silently starts a section nobody reads, and a
# duplicate one splits a section in two.
CHANGELOG_SECTIONS = ("Added", "Changed", "Deprecated", "Removed", "Fixed", "Security")


def markdown_files(repo_root: Path):
    for path in repo_root.rglob("*.md"):
        if not any(part in IGNORED_PARTS for part in path.relative_to(repo_root).parts):
            yield path


def broken_markdown_links(repo_root: Path) -> list[str]:
    failures: list[str] = []
    for markdown_path in markdown_files(repo_root):
        markdown = markdown_path.read_text(encoding="utf-8")
        for match in LINK_RE.finditer(markdown):
            destination = match.group(1).strip().strip("<>")
            if not destination or destination.startswith("#"):
                continue
            parsed = urlsplit(destination)
            if parsed.scheme or parsed.netloc:
                continue
            relative_target = unquote(parsed.path)
            if not relative_target:
                continue
            target = (markdown_path.parent / relative_target).resolve()
            if not target.exists():
                line = markdown.count("\n", 0, match.start()) + 1
                failures.append(
                    f"{markdown_path.relative_to(repo_root)}:{line}: missing link target {relative_target}"
                )
    return failures


def tracked_generated_files(repo_root: Path) -> list[str]:
    result = subprocess.run(
        ["git", "ls-files"], cwd=repo_root, check=True, capture_output=True, text=True
    )
    return [
        path
        for path in result.stdout.splitlines()
        if (repo_root / path).exists()
        and ("__pycache__/" in path or path.endswith((".pyc", ".pyo")))
    ]


def invalid_yaml_files(repo_root: Path) -> list[str]:
    failures: list[str] = []
    for path in sorted((repo_root / ".github").rglob("*.yml")):
        try:
            document = yaml.safe_load(path.read_text(encoding="utf-8"))
        except yaml.YAMLError as error:
            failures.append(f"{path.relative_to(repo_root)}: invalid YAML: {error}")
            continue

        if path.parent.name != "ISSUE_TEMPLATE" or path.name == "config.yml":
            continue
        if not isinstance(document, dict):
            failures.append(f"{path.relative_to(repo_root)}: issue form must be a mapping")
            continue
        body = document.get("body", [])
        identifiers = [item.get("id") for item in body if item.get("type") != "markdown"]
        if any(not identifier for identifier in identifiers):
            failures.append(f"{path.relative_to(repo_root)}: every input must have an id")
        if len(identifiers) != len(set(identifiers)):
            failures.append(f"{path.relative_to(repo_root)}: input ids must be unique")
    return failures


def changelog_problems(repo_root: Path) -> list[str]:
    """Structural checks on CHANGELOG.md.

    This catches a mistyped, duplicated, out-of-order or empty section. It deliberately
    does not try to judge whether an entry is filed under the right heading - no check can
    - so the companion guardrail is in CONTRIBUTING.md: read the section you are editing.
    An earlier edit in this repository deleted `### Added` outright, which left a valid
    structure and filed every addition under Fixed for two merges.
    """
    path = repo_root / "CHANGELOG.md"
    if not path.exists():
        return ["CHANGELOG.md is missing"]
    lines = path.read_text(encoding="utf-8").splitlines()

    problems: list[str] = []
    if not any(line.strip() == "## Unreleased" for line in lines):
        problems.append("CHANGELOG.md has no '## Unreleased' section")

    release = None
    seen: dict[str, list[str]] = {}
    order: dict[str, list[int]] = {}
    body: dict[tuple[str, str], bool] = {}
    current = None
    for number, line in enumerate(lines, start=1):
        stripped = line.strip()
        if stripped.startswith("## "):
            release = stripped[3:].strip()
            seen.setdefault(release, [])
            order.setdefault(release, [])
            current = None
        elif stripped.startswith("### "):
            heading = stripped[4:].strip()
            if release is None:
                problems.append(f"CHANGELOG.md:{number}: '{heading}' sits outside any release")
                continue
            if heading not in CHANGELOG_SECTIONS:
                problems.append(
                    f"CHANGELOG.md:{number}: '{heading}' is not one of "
                    f"{', '.join(CHANGELOG_SECTIONS)}"
                )
                continue
            if heading in seen[release]:
                problems.append(
                    f"CHANGELOG.md:{number}: '{heading}' appears twice under '{release}'"
                )
            seen[release].append(heading)
            order[release].append(CHANGELOG_SECTIONS.index(heading))
            current = heading
            body[(release, heading)] = False
        elif stripped.startswith("- ") and release is not None and current is not None:
            body[(release, current)] = True

    for release, positions in order.items():
        if positions != sorted(positions):
            problems.append(
                f"CHANGELOG.md: sections under '{release}' are out of order; expected "
                f"{', '.join(CHANGELOG_SECTIONS)}"
            )
    for (release, heading), has_entries in body.items():
        if not has_entries:
            problems.append(f"CHANGELOG.md: '{heading}' under '{release}' has no entries")
    return problems


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    failures = broken_markdown_links(repo_root)
    failures.extend(f"tracked generated file: {path}" for path in tracked_generated_files(repo_root))
    failures.extend(invalid_yaml_files(repo_root))
    failures.extend(changelog_problems(repo_root))
    if failures:
        print("\n".join(failures))
        return 1
    print("Repository links, changelog structure, and tracked-file hygiene checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
