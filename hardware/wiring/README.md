# Wiring Plan

**Do not assign final GPIO numbers until the exact Waveshare 2.41-B schematic is reviewed.**

Required external signals for the baseline GNSS:

| Signal | Direction | Notes |
|---|---|---|
| 3.3 V | board -> GNSS or dedicated regulator -> GNSS | verify rail/current budget |
| GND | common | keep short and solid |
| UART TX | ESP32 -> M9N RX | receiver configuration |
| UART RX | ESP32 <- M9N TX | UBX data stream |
| PPS/TIMEPULSE | ESP32 <- M9N | optional but recommended for diagnostics |

Recommended design practice:

- use a dedicated hardware UART
- avoid bit-banged serial
- route GNSS wires away from the antenna RF cable where practical
- use a locking internal connector for the final pod instead of loose Dupont jumpers
