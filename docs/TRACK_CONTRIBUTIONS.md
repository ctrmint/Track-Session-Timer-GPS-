# Public Track Definition Contributions

Track definitions affect lap and automated pit-session boundaries. They are reviewed as
safety-relevant data, not accepted as unverified convenience presets.

## Prepare a candidate

1. Capture or refine all four directed gates while stationary or from a post-session
   logged trace. Never configure the device while driving.
2. Use a stable layout-specific `track_id`; separate circuit layouts need separate files.
3. Set `provenance.source` to a public source or a clear description of an original
   measurement, record a compatible licence, and set `verified_utc` to the verification
   time. Set `geometry_status` to `provisional` for public-map research. Do not submit
   private raw location traces, account identifiers, or telemetry.

   To claim anything above `provisional`, follow the promotion process in
   [TRACK_DATABASE.md section 2.1](TRACK_DATABASE.md). In short: `independently_validated`
   needs two sources from **distinct origins** agreeing within 10 m, and two readings of
   the same database do not count as two sources. Name both origins and give the measured
   agreement in the pull request - a reviewer cannot check a claim of corroboration
   without the number.

   Be careful which second source you reach for. Lap-timing databases such as RaceChrono's
   and AiM's hold exactly this data, but they are user-contributed or vendor-maintained
   with no licence that permits redistribution, so they cannot be used here. A coordinate
   read off published circuit documentation is a fact and may be quoted; the surrounding
   map or text may not be reproduced.
4. Start at revision 1. Every geometry or timing change increments the revision and
   updates provenance. Git retains the complete review history.
5. Run the workbench before copying a candidate into `data/tracks/`:

   ```bash
   python3 -B tools/track_workbench.py validate candidate.json
   python3 -B tools/track_workbench.py import candidate.json --database-dir data/tracks
   make track-validate
   make track-definition-test
   ```

## Pull request controls

- Use one circuit/layout per pull request and state how each gate was measured.
- Include source/licence evidence and the validation commands in the pull request.
- A maintainer must review endpoint placement, direction, pit-gate meaning, geofence,
  minimum lap time, and revision progression before merge.
- Validation success proves format and geometry consistency; it does not prove physical
  placement. Public research remains `provisional` until compared with independent
  evidence or a controlled circuit test. Provisional definitions cannot be selected or
  configured for timing.
- Corrections advance the existing revision. Do not silently replace a published file or
  reuse its identifier for a different layout.
