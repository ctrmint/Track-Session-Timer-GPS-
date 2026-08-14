#!/usr/bin/env python3
"""Derive review-only gate candidates from a saved OSM Overpass snapshot.

The output is deliberately provisional. It locates the raceway point nearest the
selected pit lane and uses OSM way direction only to prepare geometry for independent
or physical validation; it never promotes a definition to timing-ready status.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path


EARTH_RADIUS_M = 6_371_000.0


def distance_m(first: dict, second: dict) -> float:
    latitude = math.radians((first["lat"] + second["lat"]) / 2.0)
    east = math.radians(second["lon"] - first["lon"]) * EARTH_RADIUS_M * math.cos(latitude)
    north = math.radians(second["lat"] - first["lat"]) * EARTH_RADIUS_M
    return math.hypot(east, north)


def bearing_deg(first: dict, second: dict) -> float:
    latitude = math.radians((first["lat"] + second["lat"]) / 2.0)
    east = math.radians(second["lon"] - first["lon"]) * math.cos(latitude)
    north = math.radians(second["lat"] - first["lat"])
    return math.degrees(math.atan2(east, north)) % 360.0


def eligible_main_way(element: dict, pit_way_id: int) -> bool:
    tags = element.get("tags", {})
    return (
        element.get("id") != pit_way_id
        and tags.get("sport") != "karting"
        and tags.get("disused") != "yes"
        and tags.get("surface") not in {"dirt", "gravel", "unpaved"}
        and "pit" not in tags.get("name", "").lower()
        and len(element.get("geometry", [])) >= 2
    )


def tangent_heading(geometry: list[dict], index: int) -> float:
    first = geometry[max(0, index - 1)]
    second = geometry[min(len(geometry) - 1, index + 1)]
    return bearing_deg(first, second)


def gate_candidate(center: dict, heading_deg: float, width_m: float) -> dict:
    return {
        "center_lat_deg": round(center["lat"], 7),
        "center_lon_deg": round(center["lon"], 7),
        "heading_deg": round(heading_deg, 3),
        "width_m": width_m,
    }


def derive(snapshot: dict, pit_way_id: int) -> dict:
    ways = snapshot.get("elements", [])
    pit = next((way for way in ways if way.get("id") == pit_way_id), None)
    if pit is None or len(pit.get("geometry", [])) < 2:
        raise ValueError(f"pit way {pit_way_id} is missing or has insufficient geometry")
    pit_geometry = pit["geometry"]
    if pit.get("tags", {}).get("oneway") == "-1":
        pit_geometry = list(reversed(pit_geometry))

    pit_midpoint = {
        "lat": sum(point["lat"] for point in pit_geometry) / len(pit_geometry),
        "lon": sum(point["lon"] for point in pit_geometry) / len(pit_geometry),
    }
    nearest: tuple[float, dict, int] | None = None
    main_way_id = 0
    for way in ways:
        if not eligible_main_way(way, pit_way_id):
            continue
        geometry = way["geometry"]
        for index, point in enumerate(geometry):
            candidate = (distance_m(pit_midpoint, point), way, index)
            if nearest is None or candidate[0] < nearest[0]:
                nearest = candidate
                main_way_id = way["id"]
    if nearest is None:
        raise ValueError("no eligible paved motor raceway was found near the pit lane")

    _, main_way, main_index = nearest
    main_geometry = main_way["geometry"]
    main_heading = tangent_heading(main_geometry, main_index)
    start_finish = gate_candidate(main_geometry[main_index], main_heading, 20.0)
    pit_entry_heading = bearing_deg(
        pit_geometry[0], pit_geometry[min(2, len(pit_geometry) - 1)]
    )
    pit_exit_heading = bearing_deg(
        pit_geometry[max(0, len(pit_geometry) - 3)], pit_geometry[-1]
    )
    return {
        "geometry_status": "provisional",
        "source_way_ids": {"main": main_way_id, "pit": pit_way_id},
        "gates": {
            "start": start_finish,
            "finish": start_finish,
            "pit_entry": gate_candidate(pit_geometry[0], pit_entry_heading, 12.0),
            "pit_exit": gate_candidate(pit_geometry[-1], pit_exit_heading, 12.0),
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("snapshot", type=Path)
    parser.add_argument("--pit-way", type=int, required=True)
    arguments = parser.parse_args()
    snapshot = json.loads(arguments.snapshot.read_text(encoding="utf-8"))
    print(json.dumps(derive(snapshot, arguments.pit_way), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
