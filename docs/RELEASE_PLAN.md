# Release Plan

## Versioning

Use Semantic Versioning from the first tagged firmware artefact.

Pre-1.0 releases may break configuration and log formats, but schema versions must still be explicit.

## Release artefacts

A release should include:

- firmware binary/flash instructions
- source tag
- supported hardware revision
- hardware BOM revision
- track database schema version
- release notes
- known limitations
- migration notes for settings/log formats

## Release gates

### Alpha

- builds reproducibly
- basic hardware stable
- GNSS logging stable
- not intended for track dependence

### Beta

- live lap timing operational
- session timer ported
- host replay matches embedded events
- limited track testing complete

### Release candidate

- enclosure stable
- no critical defects
- multi-circuit tests
- power-loss and soak testing complete

### 1.0

- documented baseline hardware
- repeatable install/flash process
- defined track-file workflow
- known timing limitations explicitly documented
