#!/usr/bin/env python3
"""Build the deterministic, provenance-gated UK offline track pack."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import tempfile
import zipfile

try:
    from tools.track_workbench import create_definition, write_definition
except ModuleNotFoundError:  # Direct `python tools/build_uk_track_pack.py` execution.
    from track_workbench import create_definition, write_definition


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = REPO_ROOT / "data" / "track-packs" / "uk" / "manifest.json"
DEFAULT_OUTPUT = REPO_ROOT / "build" / "track-pack" / "uk"
TRACK_ID_PATTERN = re.compile(r"^[A-Za-z0-9._-]{1,47}$")
NATIONS = {"England", "Scotland", "Wales", "Northern Ireland"}
GATE_NAMES = ("start", "finish", "pit_entry", "pit_exit")


class TrackPackError(ValueError):
    """Raised when the source manifest cannot produce a safe package."""


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise TrackPackError(message)


def validate_manifest(manifest: dict) -> None:
    _require(manifest.get("schema_version") == 1, "unsupported manifest schema")
    _require(manifest.get("pack_id") == "uk-major-circuits", "unexpected pack id")
    _require(manifest.get("revision", 0) > 0, "pack revision must be positive")
    _require(manifest.get("data_license") == "ODbL-1.0", "pack must retain ODbL")
    _require("OpenStreetMap" in manifest.get("attribution", ""), "OSM attribution missing")
    _require(
        isinstance(manifest.get("scope_exclusions"), list)
        and manifest["scope_exclusions"],
        "scope exclusions must be explicit",
    )
    venues = manifest.get("venues")
    _require(isinstance(venues, list) and venues, "venues must be a non-empty list")

    venue_ids: set[str] = set()
    track_ids: set[str] = set()
    represented_nations: set[str] = set()
    for venue_index, venue in enumerate(venues):
        path = f"venues.{venue_index}"
        venue_id = venue.get("venue_id")
        _require(isinstance(venue_id, str) and venue_id, f"{path}.venue_id missing")
        _require(venue_id not in venue_ids, f"{path}.venue_id duplicates {venue_id}")
        venue_ids.add(venue_id)
        nation = venue.get("nation")
        _require(nation in NATIONS, f"{path}.nation is unsupported")
        represented_nations.add(nation)
        reference = venue.get("reference")
        _require(
            isinstance(reference, list)
            and len(reference) == 2
            and -90 <= reference[0] <= 90
            and -180 <= reference[1] <= 180,
            f"{path}.reference is invalid",
        )
        _require(
            0 < venue.get("geofence_radius_m", 0) <= 25_000,
            f"{path}.geofence_radius_m is invalid",
        )
        for source_field in ("osm_venue", "official_source"):
            _require(
                str(venue.get(source_field, "")).startswith("https://"),
                f"{path}.{source_field} must be an HTTPS source",
            )

        profiles = venue.get("gate_profiles")
        _require(isinstance(profiles, dict), f"{path}.gate_profiles must be an object")
        for profile_name, profile in profiles.items():
            source_ids = profile.get("source_way_ids")
            _require(
                isinstance(source_ids, list)
                and len(source_ids) == 2
                and all(isinstance(value, int) and value > 0 for value in source_ids),
                f"{path}.gate_profiles.{profile_name}.source_way_ids is invalid",
            )
            for gate_name in GATE_NAMES:
                gate = profile.get(gate_name)
                _require(
                    isinstance(gate, list)
                    and len(gate) == 4
                    and -90 <= gate[0] <= 90
                    and -180 <= gate[1] <= 180
                    and 0 <= gate[2] < 360
                    and 1 <= gate[3] <= 1_000,
                    f"{path}.gate_profiles.{profile_name}.{gate_name} is invalid",
                )

        layouts = venue.get("layouts")
        _require(isinstance(layouts, list) and layouts, f"{path}.layouts missing")
        for layout_index, layout in enumerate(layouts):
            layout_path = f"{path}.layouts.{layout_index}"
            track_id = layout.get("track_id")
            _require(
                isinstance(track_id, str) and TRACK_ID_PATTERN.fullmatch(track_id),
                f"{layout_path}.track_id is invalid",
            )
            _require(track_id not in track_ids, f"{layout_path}.track_id duplicates {track_id}")
            track_ids.add(track_id)
            _require(
                isinstance(layout.get("name"), str) and 0 < len(layout["name"]) <= 63,
                f"{layout_path}.name is invalid",
            )
            _require(
                0 < layout.get("minimum_lap_time_s", 0) <= 3_600,
                f"{layout_path}.minimum_lap_time_s is invalid",
            )
            has_blocker = bool(layout.get("blocker"))
            has_profile = bool(layout.get("gate_profile"))
            _require(
                has_blocker != has_profile,
                f"{layout_path} must have exactly one gate_profile or blocker",
            )
            if has_profile:
                _require(
                    layout["gate_profile"] in profiles,
                    f"{layout_path}.gate_profile does not exist",
                )
    _require(represented_nations == NATIONS, "manifest must cover all four UK nations")


# Promoting geometry is the one thing in this generator that can turn unverified data
# into a lap time a driver will believe, so the rules are enforced here rather than left
# to review. Derivation alone always yields "provisional"; only an explicit, evidenced
# validation record can raise it, and a record that does not meet these rules stops the
# build rather than quietly producing a provisional pack nobody notices.
PROMOTED_STATUSES = frozenset(
    {"device_captured", "independently_validated", "physically_validated"}
)

# Two sources that disagree by more than this cannot promote a gate.
#
# Ten metres is 0.16 s at 62.6 m/s, the fastest a UK circuit is driven. A start/finish
# line displaced along the straight shifts every lap by the same amount, so lap durations
# and lap-to-lap comparisons are unaffected; what it changes is absolute agreement with
# another timing system. GPS lap timing carries roughly 0.15-0.25 s of error of its own,
# so demanding better agreement than that would be false precision - and demanding worse
# would admit a line on the wrong part of the circuit.
MAXIMUM_SOURCE_DISAGREEMENT_M = 10.0


def _validation_for(profile: dict, path: str) -> tuple[str, str | None, str | None]:
    """Return (geometry_status, evidence_text, verified_utc) for a gate profile."""
    validation = profile.get("validation")
    if validation is None:
        return "provisional", None, None

    status = validation.get("geometry_status")
    _require(
        status in PROMOTED_STATUSES,
        f"{path}.validation.geometry_status must be one of {sorted(PROMOTED_STATUSES)}",
    )
    verified_utc = validation.get("verified_utc")
    _require(
        isinstance(verified_utc, str) and verified_utc.endswith("Z"),
        f"{path}.validation.verified_utc must be an RFC3339 UTC timestamp",
    )

    sources = validation.get("sources") or []
    _require(
        len(sources) >= 2,
        f"{path}.validation needs at least two sources; one source is not corroboration",
    )
    origins = set()
    for index, source in enumerate(sources):
        source_path = f"{path}.validation.sources[{index}]"
        for field in ("name", "url", "origin"):
            _require(
                isinstance(source.get(field), str) and source[field],
                f"{source_path}.{field} is required",
            )
        _require(
            source["url"].startswith("https://"),
            f"{source_path}.url must be an HTTPS source",
        )
        origins.add(source["origin"])
    # Two readings of the same database are one source read twice.
    _require(
        len(origins) >= 2,
        f"{path}.validation sources must come from at least two distinct origins",
    )

    agreement = validation.get("agreement_m")
    _require(
        isinstance(agreement, (int, float)) and 0.0 <= float(agreement),
        f"{path}.validation.agreement_m is required",
    )
    _require(
        float(agreement) <= MAXIMUM_SOURCE_DISAGREEMENT_M,
        f"{path}.validation.agreement_m of {agreement} m exceeds the "
        f"{MAXIMUM_SOURCE_DISAGREEMENT_M} m limit",
    )

    # Compact by construction: provenance.source is capped at 127 characters by the
    # definition schema, and the full citation - every source, its URL and its licence -
    # travels with the pack in source-manifest.json, so nothing is lost by being brief
    # here.
    evidence = f"{len(origins)} origins agreeing to {float(agreement):.1f} m"
    return status, evidence, verified_utc


def _definition_for(manifest: dict, venue: dict, layout: dict) -> dict:
    profile = venue["gate_profiles"][layout["gate_profile"]]
    gate_specs = {
        gate_name: tuple(profile[gate_name]) for gate_name in GATE_NAMES
    }
    source_ids = profile["source_way_ids"]
    status, evidence, verified_utc = _validation_for(
        profile, f"{venue['venue_id']}.{layout['gate_profile']}"
    )
    source = f"OpenStreetMap ways {source_ids[0]}/{source_ids[1]}; UK pack manifest"
    if evidence is not None:
        source = f"{source}; validated: {evidence}"
    return create_definition(
        track_id=layout["track_id"],
        name=layout["name"],
        country="GB",
        source=source,
        license_name=manifest["data_license"],
        verified_utc=verified_utc or manifest["researched_utc"],
        reference_lat_deg=venue["reference"][0],
        reference_lon_deg=venue["reference"][1],
        geofence_radius_m=venue["geofence_radius_m"],
        gate_specs=gate_specs,
        minimum_lap_time_s=layout["minimum_lap_time_s"],
        geometry_status=status,
    )


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _write_json(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def _write_zip(output_dir: Path, archive_path: Path) -> None:
    temporary_archive = archive_path.with_suffix(".tmp")
    with zipfile.ZipFile(temporary_archive, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(output_dir.rglob("*.json")):
            info = zipfile.ZipInfo(path.relative_to(output_dir).as_posix())
            info.date_time = (1980, 1, 1, 0, 0, 0)
            info.external_attr = 0o644 << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, path.read_bytes())
    temporary_archive.replace(archive_path)


def build_pack(manifest_path: Path, output_dir: Path) -> dict:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    validate_manifest(manifest)
    output_dir.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".uk-track-pack-", dir=output_dir.parent))
    try:
        definitions_dir = temporary / "definitions"
        definitions_dir.mkdir()
        packaged_layouts: list[dict] = []
        generated_count = 0
        timing_ready_count = 0
        blocked_count = 0
        for venue in manifest["venues"]:
            for layout in venue["layouts"]:
                packaged = {
                    "track_id": layout["track_id"],
                    "name": layout["name"],
                    "venue_id": venue["venue_id"],
                    "nation": venue["nation"],
                    "official_source": venue["official_source"],
                    "osm_venue": venue["osm_venue"],
                }
                if layout.get("blocker"):
                    packaged.update(
                        geometry_status="blocked",
                        timing_ready=False,
                        blocker=layout["blocker"],
                    )
                    blocked_count += 1
                else:
                    definition = _definition_for(manifest, venue, layout)
                    definition_path = definitions_dir / f"{layout['track_id']}.json"
                    write_definition(definition_path, definition)
                    status = definition["provenance"]["geometry_status"]
                    packaged.update(
                        geometry_status=status,
                        timing_ready=status in PROMOTED_STATUSES,
                        definition=f"definitions/{definition_path.name}",
                        sha256=_sha256(definition_path),
                        source_way_ids=venue["gate_profiles"][layout["gate_profile"]][
                            "source_way_ids"
                        ],
                    )
                    generated_count += 1
                    if packaged["timing_ready"]:
                        timing_ready_count += 1
                packaged_layouts.append(packaged)

        package_manifest = {
            "schema_version": 1,
            "pack_id": manifest["pack_id"],
            "revision": manifest["revision"],
            "researched_utc": manifest["researched_utc"],
            "data_license": manifest["data_license"],
            "attribution": manifest["attribution"],
            "policy": manifest["policy"],
            "scope_exclusions": manifest.get("scope_exclusions", []),
            "layout_count": len(packaged_layouts),
            "definition_count": generated_count,
            "timing_ready_count": timing_ready_count,
            "blocked_count": blocked_count,
            "layouts": packaged_layouts,
        }
        _write_json(temporary / "pack-manifest.json", package_manifest)
        shutil.copyfile(manifest_path, temporary / "source-manifest.json")
        if output_dir.exists():
            shutil.rmtree(output_dir)
        temporary.replace(output_dir)
        archive_path = output_dir.parent / "uk-track-pack-v1.zip"
        _write_zip(output_dir, archive_path)
        package_manifest["archive"] = str(archive_path)
        package_manifest["archive_sha256"] = _sha256(archive_path)
        return package_manifest
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    arguments = parser.parse_args()
    result = build_pack(arguments.manifest, arguments.output)
    print(
        f"Built {result['definition_count']} definition(s), of which "
        f"{result['timing_ready_count']} are timing-ready; "
        f"documented {result['blocked_count']} blocker(s) across "
        f"{result['layout_count']} UK layout(s)"
    )
    print(f"Archive: {result['archive']} ({result['archive_sha256']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
