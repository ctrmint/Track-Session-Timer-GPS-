# Migration from the Existing TrackSessionTimer

## Principle

Port the proven product behaviour, not the RP2040-specific implementation.

The existing repository remains the behavioural reference for the session-timer experience. The new project should avoid copying assumptions tied to the original 240 x 240 GC9A01A display, MicroPython framebuffer, RP2040 pin map or polling limits.

## Behaviour to preserve or deliberately reimplement

### Session workflow

- Ready state
- configurable track duration
- configurable rest duration where still useful
- track session countdown
- overtime state
- deliberate stop/finish interaction
- post-session review

### Driver feedback

- strong colour progression through the session
- large stable timer digits
- high-contrast text selection
- clear overtime presentation

### IMU features

Evaluate and retain where they add value:

- G meter
- directional acceleration/braking/left/right peaks
- launch-related detection
- fixed/automatic orientation
- stationary auto-dim behaviour

### Settings

Recreate settings with a versioned new format rather than preserving the original file layout byte-for-byte.

## Behaviour to redesign

### Display rendering

Do not port the 240 x 240 full-frame assumptions. The 600 x 450 display needs a measured LVGL/buffer strategy.

### Touch interaction

The larger rectangular screen allows clearer layouts and larger explicit controls. Do not preserve round-screen gestures only because the old hardware required them.

### Session summary storage

The old project intentionally bounded review data in RAM. The new design has microSD logging and should persist session data safely, while still keeping the active timing path independent of storage.

### Timing source

Session countdown can continue to use the MCU monotonic clock. GPS lap events use GNSS measurement time.

## Do not port

- RP2040 pin assignments
- GC9A01A-specific display code
- CST816S-specific touch code
- framebuffer-size assumptions
- MicroPython scheduling workarounds
- constraints that existed only because the previous board lacked high-rate GNSS and SD logging architecture

## Suggested migration sequence

1. Finish display/GNSS/timing architecture first.
2. Create host tests for the new session state model.
3. Use the old UI behaviour as a reference while implementing new screens.
4. Port one behaviour at a time and add acceptance tests.
5. Compare the old and new devices side by side before removing any original capability.
