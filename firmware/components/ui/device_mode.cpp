#include "track_timer/ui/device_mode.hpp"

namespace track_timer::ui {

ModeVisibility visibility_for(const DeviceMode mode) noexcept
{
    switch (mode) {
    case DeviceMode::track_day:
        // Session countdown only. No lap time, no delta, on circuit.
        return {true, false, false, false};
    case DeviceMode::race:
        return {true, true, true, false};
    case DeviceMode::g_only:
        // The G meter owns the whole panel; no timer, no laps.
        return {false, false, false, true};
    }
    return {true, false, false, false};
}

DeviceMode mode_from_settings(const settings::DeviceSettings& settings) noexcept
{
    // g_meter wins over the trackday flag, so the ambiguous combination that the UI
    // never writes still resolves to something definite.
    if (settings.operating_mode == settings::OperatingMode::g_meter) {
        return DeviceMode::g_only;
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
    }
    return "";
}

}  // namespace track_timer::ui
