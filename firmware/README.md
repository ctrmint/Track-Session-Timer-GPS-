# Firmware

Minimal ESP-IDF bootstrap project.

The first commit intentionally avoids pretending the Waveshare board support is already solved. Build and flash this project first, then implement the board support layer in a dedicated issue.

```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

Planned component folders after bring-up:

```text
components/
  board/
  gnss/
  timing/
  track/
  session/
  logger/
  ui/
  diagnostics/
```
