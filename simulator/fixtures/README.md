# GNSS simulator fixtures

Version 1 fixtures are UTF-8 CSV files with:

1. the exact marker `# track-session-timer-gnss-fixture-v1`
2. a non-empty `# name=...` identifier
3. the fixed version 1 header
4. at least two rows with strictly increasing measurement timestamps

Run `make simulator-fixture-validate` after adding or editing a fixture. Both the
Python validator and the C++ replay loader reject unsupported versions, malformed
rows, non-monotonic timestamps, and out-of-range receiver values.

`recorded_reference_v1.csv` is a checked-in, prerecorded reference stream derived
from the repository's synthetic test loop. It exercises the same file-backed path
that receiver captures will use, but it is not represented as an RF-quality or
hardware-accuracy sample. Physical receiver captures can be added in this format
after the board arrives, with their provenance and privacy treatment documented.
