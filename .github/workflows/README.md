# Workflows

`host-and-firmware-checks.yml` is the required pull-request baseline:

- Python 3.13 host tests
- track schema/fixture validation
- Markdown and generated-file hygiene
- host C++17 domain-contract compilation
- deterministic simulator model tests
- a headless 600 x 450 LVGL/SDL simulator build and smoke test
- ESP32-S3 firmware compilation with ESP-IDF v6.0.2

Every job sets `timeout-minutes`. Without it a job inherits GitHub's six-hour default, so a
network stall - `apt-get` waiting on an unreachable mirror, in the case that prompted this -
blocks a pull request for the rest of the day instead of failing visibly. Network-dependent
steps additionally time out and retry individually, because a retry alone does nothing for a
command that never returns.

The workflow pins release families rather than using moving `latest` tags. Update the
Python, LVGL, ESP-IDF, dependency, and local documentation pins together in one reviewed PR.
