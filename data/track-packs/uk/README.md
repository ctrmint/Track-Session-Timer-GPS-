# UK major-circuit source manifest

`manifest.json` is the source of the deterministic offline pack built by
`make uk-track-pack`. It covers 35 standard layouts at 18 permanent paved car venues in
England, Scotland, Wales, and Northern Ireland.

The gate candidates are a derivative database of OpenStreetMap data and are distributed
under the Open Database License 1.0 (ODbL). Required attribution: **Contains information
from OpenStreetMap, available under ODbL 1.0**. See
<https://www.openstreetmap.org/copyright>. Official venue links establish layout identity;
no official map artwork or proprietary coordinate data is redistributed.

Every generated definition is `provisional`, is reported as `timing_ready: false`, and
is rejected by both track selection and the timing configuration adapter. The candidates
only seed offline discovery and the safe eight-endpoint capture workflow. A later
revision may become timing-ready only after all four gates are independently or
physically validated. Entries without a defensible public pit lane carry an explicit
blocker and generate no definition.
