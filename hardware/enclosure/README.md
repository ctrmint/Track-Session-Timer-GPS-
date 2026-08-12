# Enclosure Requirements

## Prototype 1

Use the Waveshare `-B` case and add a rear pod rather than replacing the front enclosure immediately.

The rear pod should provide:

- mounting for GPS-15712
- protected U.FL bend radius
- SMA bulkhead or mechanically supported antenna cable exit
- access to the board USB-C port
- access to microSD if practical
- strain relief for internal wires
- mounting face for the vehicle bracket
- ventilation without exposing electronics to easy debris ingress

## Design constraints

- no load on U.FL connector from external antenna cable
- no loose components under vibration
- no sharp edge against wiring
- screen angle adjustable or chosen after cockpit mock-up
- avoid placing a metal bracket directly over the ESP32 antenna unless Wi-Fi/BLE is deliberately irrelevant and testing shows no other consequence

## Later revision

Once the prototype is validated, consider a single rear cover that integrates:

- GNSS daughterboard
- protected 5 V input
- clean 3.3 V GNSS regulator
- SMA connector
- optional external button connector
