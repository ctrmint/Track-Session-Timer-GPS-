#!/usr/bin/env python3
"""Refuse to run the checks against a Python that cannot validate what they assert.

The host suite depends on more than the presence of its packages. `jsonschema` only
enforces a `format` keyword when a checker for that format is installed, and it stays
silent when one is not: without `rfc3339-validator`, `format: date-time` is not checked
and `provenance.verified_utc` will accept any string at all. That field is what the track
provenance gate reads, so a missing dependency does not fail the validators - it quietly
widens what they accept.

That is why this runs before the suite rather than being left to a test. A test can only
report the symptom, and the symptom here is one confusing assertion in a schema test while
every tool that validates a track definition carries on looking like it works.
"""

from __future__ import annotations

import importlib.metadata
from pathlib import Path
import re
import sys


REPO_ROOT = Path(__file__).resolve().parents[1]
REQUIREMENTS = REPO_ROOT / "requirements-dev.txt"
PIN_PATTERN = re.compile(r"^([A-Za-z0-9._-]+)==([^\s#]+)")

HINT = """
Create the pinned environment the checks expect:

    python3 -m venv .venv
    .venv/bin/python -m pip install -r requirements-dev.txt

The Makefile picks up .venv automatically, so there is no need to activate it.
"""


def _pinned() -> list[tuple[str, str]]:
    pins = []
    for line in REQUIREMENTS.read_text(encoding="utf-8").splitlines():
        match = PIN_PATTERN.match(line.strip())
        if match:
            pins.append((match.group(1), match.group(2)))
    return pins


def main() -> int:
    failures: list[str] = []
    drifts: list[str] = []

    for name, expected in _pinned():
        try:
            installed = importlib.metadata.version(name)
        except importlib.metadata.PackageNotFoundError:
            failures.append(f"{name} is not installed (requirements-dev.txt pins {expected})")
            continue
        if installed != expected:
            drifts.append(f"{name} {installed} installed, {expected} pinned")

    # The capability, not just the package: an installed format library that jsonschema
    # has not registered is the same failure with a more convincing appearance.
    try:
        from jsonschema import FormatChecker

        if "date-time" not in FormatChecker().checkers:
            failures.append(
                "jsonschema has no date-time format checker, so provenance.verified_utc "
                "is not validated at all"
            )
    except ImportError as error:  # jsonschema itself is missing; already reported above.
        failures.append(f"jsonschema is unusable: {error}")

    for drift in drifts:
        print(f"warning: {drift}", file=sys.stderr)

    if failures:
        print(f"{sys.executable} cannot run the host checks:", file=sys.stderr)
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        print(HINT, file=sys.stderr)
        return 1

    print(f"python environment ok: {sys.executable}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
