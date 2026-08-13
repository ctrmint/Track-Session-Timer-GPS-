# Initial Development Backlog

The live GitHub issue tracker is the source of truth. This snapshot was restored from
the repository's 61 published issues after the original `planning/` directory was
omitted during repository upload.

## Delivery epics

| Epic | Outcome | Milestone | Sub-issues |
|---|---|---|---:|
| [#1](https://github.com/ctrmint/Track-Session-Timer-GPS-/issues/1) | Repository and toolchain bootstrap | M0 — Repository and toolchain bootstrap | 5 |
| [#7](https://github.com/ctrmint/Track-Session-Timer-GPS-/issues/7) | Waveshare board support and peripheral bring-up | M1 — Display board bring-up | 5 |
| [#13](https://github.com/ctrmint/Track-Session-Timer-GPS-/issues/13) | High-rate GNSS acquisition and diagnostics | M2 — GNSS bring-up | 5 |
| [#19](https://github.com/ctrmint/Track-Session-Timer-GPS-/issues/19) | Host and embedded lap timing engine | M3–M4 — Lap timing engine | 6 |
| [#26](https://github.com/ctrmint/Track-Session-Timer-GPS-/issues/26) | Session workflow and driver-facing UI | M5–M6 — Session and driver UI | 6 |
| [#33](https://github.com/ctrmint/Track-Session-Timer-GPS-/issues/33) | Versioned track database and track setup | M7 — Track database | 4 |
| [#38](https://github.com/ctrmint/Track-Session-Timer-GPS-/issues/38) | Fault-tolerant session logging and deterministic replay | M8 — Logging and replay | 5 |
| [#44](https://github.com/ctrmint/Track-Session-Timer-GPS-/issues/44) | Power, enclosure, antenna, and vehicle integration | M9 — Vehicle integration | 5 |
| [#50](https://github.com/ctrmint/Track-Session-Timer-GPS-/issues/50) | System validation, track testing, and v1.0 release | M10 — Validation and v1.0 | 5 |
| [#56](https://github.com/ctrmint/Track-Session-Timer-GPS-/issues/56) | Post-1.0 analysis, track management, and research roadmap | Post-1.0 roadmap | 5 |

The epics use native GitHub sub-issues and sequential blocked-by relationships. Work
should normally complete a milestone exit gate before advancing to its dependent epic.

See [issues.csv](issues.csv) for the complete issue snapshot and
[labels.csv](labels.csv) for the label definitions.
