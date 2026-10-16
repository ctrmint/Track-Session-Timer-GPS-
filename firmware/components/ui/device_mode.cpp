#include "track_timer/ui/device_mode.hpp"

namespace track_timer::ui {

ModeVisibility visibility_for(const DeviceMode mode) noexcept
{
    switch (mode) {
    case DeviceMode::track_day:
        // Session countdown only. No lap time, no delta, on circuit.
        return {true, false, false, false, false};
    case DeviceMode::race:
        return {true, true, true, false, false};
    case DeviceMode::g_only:
        // The G meter owns the whole panel; no timer, no laps.
        return {false, false, false, true, false};
    case DeviceMode::speed_only:
        // Road speed alone. gnss_data is set because that is where the number comes from,
        // not because a position is drawn; like the two below it, this is not a session
        // view and must not offer a timer.
        return {false, false, false, false, true};
    case DeviceMode::compass:
        // A compass card and nothing else. gnss_data again marks where the number comes
        // from rather than what is drawn, and again this is not a session view.
        return {false, false, false, false, true};
    case DeviceMode::acceleration:
        // A measurement, and nothing that could be mistaken for timing a session.
        return {false, false, false, false, true};
    case DeviceMode::gps_only:
        // A diagnostic view of the receiver alone. Showing a session timer here would
        // invite using it to time something, which is the one thing this mode is not for.
        return {false, false, false, false, true};
    case DeviceMode::clock:
        // A clock and nothing else. It reads the receiver's time, which is why gnss_data is
        // set, but it is not a session view and must not offer a timer: a device showing a
        // clock is not a device being used to time something.
        return {false, false, false, false, true};
    }
    return {true, false, false, false, false};
}

DeviceMode mode_from_settings(const settings::DeviceSettings& settings) noexcept
{
    // The operating mode wins over the trackday flag, so the ambiguous combinations that
    // the UI never writes still resolve to something definite.
    if (settings.operating_mode == settings::OperatingMode::g_meter) {
        return DeviceMode::g_only;
    }
    if (settings.operating_mode == settings::OperatingMode::speed_only) {
        return DeviceMode::speed_only;
    }
    if (settings.operating_mode == settings::OperatingMode::compass) {
        return DeviceMode::compass;
    }
    if (settings.operating_mode == settings::OperatingMode::acceleration) {
        return DeviceMode::acceleration;
    }
    if (settings.operating_mode == settings::OperatingMode::gps_only) {
        return DeviceMode::gps_only;
    }
    if (settings.operating_mode == settings::OperatingMode::clock) {
        return DeviceMode::clock;
    }
    return settings.trackday_mode_enabled ? DeviceMode::track_day : DeviceMode::race;
}

void apply_mode(const DeviceMode mode, settings::DeviceSettings& settings) noexcept
{
    switch (mode) {
    case DeviceMode::track_day:
        settings.operating_mode = settings::OperatingMode::timer;
        settings.trackday_mode_enabled = true;
        break;
    case DeviceMode::race:
        settings.operating_mode = settings::OperatingMode::timer;
        settings.trackday_mode_enabled = false;
        break;
    case DeviceMode::g_only:
        settings.operating_mode = settings::OperatingMode::g_meter;
        settings.trackday_mode_enabled = false;
        break;
    case DeviceMode::speed_only:
        settings.operating_mode = settings::OperatingMode::speed_only;
        settings.trackday_mode_enabled = false;
        break;
    case DeviceMode::compass:
        settings.operating_mode = settings::OperatingMode::compass;
        settings.trackday_mode_enabled = false;
        break;
    case DeviceMode::acceleration:
        settings.operating_mode = settings::OperatingMode::acceleration;
        settings.trackday_mode_enabled = false;
        break;
    case DeviceMode::gps_only:
        settings.operating_mode = settings::OperatingMode::gps_only;
        settings.trackday_mode_enabled = false;
        break;
    case DeviceMode::clock:
        settings.operating_mode = settings::OperatingMode::clock;
        settings.trackday_mode_enabled = false;
        break;
    }
}

const char* device_mode_name(const DeviceMode mode) noexcept
{
    switch (mode) {
    case DeviceMode::track_day:
        return "track-day";
    case DeviceMode::race:
        return "race";
    case DeviceMode::g_only:
        return "g-only";
    case DeviceMode::speed_only:
        return "speed-only";
    case DeviceMode::compass:
        return "compass";
    case DeviceMode::acceleration:
        return "acceleration";
    case DeviceMode::gps_only:
        return "gps-only";
    case DeviceMode::clock:
        return "clock";
    }
    return "unknown";
}

const char* device_mode_label(const DeviceMode mode) noexcept
{
    switch (mode) {
    case DeviceMode::track_day:
        return "TRACK DAY";
    case DeviceMode::race:
        return "RACE";
    case DeviceMode::g_only:
        return "G-ONLY";
    case DeviceMode::speed_only:
        return "SPEED";
    case DeviceMode::compass:
        return "COMPASS";
    case DeviceMode::acceleration:
        return "0-X";
    case DeviceMode::gps_only:
        return "GPS ONLY";
    case DeviceMode::clock:
        return "CLOCK";
    }
    return "";
}

const char* device_mode_summary(const DeviceMode mode) noexcept
{
    switch (mode) {
    case DeviceMode::track_day:
        return "Session timer only. Laps recorded for review after the session.";
    case DeviceMode::race:
        return "Lap times, delta and session remaining.";
    case DeviceMode::g_only:
        return "Graphical G meter only.";
    case DeviceMode::speed_only:
        return "Road speed in mph, as large as the panel allows.";
    case DeviceMode::compass:
        return "Course over ground. Needs movement: a parked car has no heading.";
    case DeviceMode::acceleration:
        return "Standing start to a target speed, set in Setup.";
    case DeviceMode::gps_only:
        return "Road speed, position and receiver data. Diagnosis, not timing.";
    case DeviceMode::clock:
        return "A 24-hour GMT clock from the receiver's own UTC time.";
    }
    return "";
}

}  // namespace track_timer::ui
