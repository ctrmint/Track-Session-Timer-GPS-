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
    case DeviceMode::gps_only:
        // A diagnostic view of the receiver alone. Showing a session timer here would
        // invite using it to time something, which is the one thing this mode is not for.
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
    if (settings.operating_mode == settings::OperatingMode::gps_only) {
        return DeviceMode::gps_only;
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
    case DeviceMode::gps_only:
        settings.operating_mode = settings::OperatingMode::gps_only;
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
    case DeviceMode::gps_only:
        return "gps-only";
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
    case DeviceMode::gps_only:
        return "GPS ONLY";
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
    case DeviceMode::gps_only:
        return "Road speed, position and receiver data. Diagnosis, not timing.";
    }
    return "";
}

}  // namespace track_timer::ui
