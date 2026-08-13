# Workflows

`host-and-firmware-checks.yml` is the required pull-request baseline:

- Python 3.13 host tests
- track schema/fixture validation
- Markdown and generated-file hygiene
- host C++17 domain-contract compilation
- ESP32-S3 firmware compilation with ESP-IDF v6.0.2

The workflow pins release families rather than using moving `latest` tags. Update the
Python, ESP-IDF, dependency, and local documentation pins together in one reviewed PR.
