# Firmware component boundaries

These directories establish dependency direction before hardware drivers are added.
Only `domain` contains product value contracts today; the remaining components have
link anchors so ESP-IDF verifies the intended graph.

| Component | Owns | Direct dependencies |
|---|---|---|
| `domain` | Fixed-size cross-task values and queue capacities | none |
| `board` | Display, touch, IMU, SD, RTC, power, UART abstractions | `domain` |
| `gnss` | UBX transport, receiver configuration, fix validation | `domain`, `board` |
| `track` | Track loading, validation, projection inputs | `domain` |
| `timing` | Crossing geometry and lap state | `domain`, `track` |
| `session` | Session countdown and lifecycle | `domain`, `timing` |
| `logger` | Bounded logging queue and serialization | `domain`, `board` |
| `ui` | LVGL presentation and user input | `domain`, `session`, `board` |
| `diagnostics` | Health/counter aggregation | all service components |

## Queue ownership

| Queue | Producer | Consumer | Capacity |
|---|---|---|---:|
| GNSS fixes | `gnss` | `timing` | 64 fixes (2.56 s at 25 Hz) |
| Lap events | `timing` | `session` | 16 events |
| Log records | GNSS/timing/session | `logger` | 256 records |
| UI snapshots | `session`/diagnostics | `ui` | 2 snapshots |

Every eventual queue implementation must be bounded. Overflow must increment an
observable counter; it must never silently discard the oldest unprocessed item.
UI rendering and SD writes must not execute in the GNSS or timing tasks.
