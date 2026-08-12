# Hardware Workspace

This directory is for hardware artefacts that will evolve after the first prototype.

```text
hardware/
  bom.csv
  enclosure/
  wiring/
```

## Prototype strategy

Do not design a custom main PCB first.

Use:

1. Waveshare ESP32-S3-Touch-AMOLED-2.41-B
2. SparkFun NEO-M9N breakout
3. external antenna
4. temporary but mechanically secure interconnect
5. 3D printed rear pod

Only after track timing is proven should the GNSS and power circuitry be consolidated onto a daughterboard.
