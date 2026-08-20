#include "track_timer/ui/session_trigger.hpp"

namespace track_timer::ui {

void apply_trigger(const settings::SessionTrigger trigger,
                   settings::DeviceSettings& settings) noexcept
{
    settings.session_trigger = trigger;
    // The trigger is the only thing that writes this now, so the tested gate automation
    // keeps reading the boolean it always has without knowing a trigger exists.
    settings.pit_exit_auto_start_enabled = trigger == settings::SessionTrigger::gps;
}

settings::SessionTrigger trigger_from_settings(
    const settings::DeviceSettings& settings) noexcept
{
    return settings.session_trigger;
}

const char* session_trigger_name(const settings::SessionTrigger trigger) noexcept
{
    switch (trigger) {
    case settings::SessionTrigger::manual:
        return "manual";
    case settings::SessionTrigger::imu:
        return "imu";
    case settings::SessionTrigger::gps:
        return "gps";
    }
    return "manual";
}

const char* session_trigger_label(const settings::SessionTrigger trigger) noexcept
{
    switch (trigger) {
    case settings::SessionTrigger::manual:
        return "MANUAL";
    case settings::SessionTrigger::imu:
        return "IMU";
    case settings::SessionTrigger::gps:
        return "GPS";
    }
    return "MANUAL";
}

const char* session_trigger_summary(const settings::SessionTrigger trigger) noexcept
{
    switch (trigger) {
    case settings::SessionTrigger::manual:
        return "START BEGINS THE SESSION";
    case settings::SessionTrigger::imu:
        return "STARTS ON LAUNCH";
    case settings::SessionTrigger::gps:
        return "STARTS AT THE LINE";
    }
    return "START BEGINS THE SESSION";
}

TriggerReadiness trigger_readiness(const settings::SessionTrigger trigger,
                                   const std::uint16_t launch_milli_g,
                                   const bool imu_calibrated,
                                   const bool gnss_available) noexcept
{
    switch (trigger) {
    case settings::SessionTrigger::manual:
        return {true, ""};
    case settings::SessionTrigger::imu:
        if (launch_milli_g == 0) {
            // Otherwise the device waits for a threshold that cannot be reached, with
            // nothing on screen to say why.
            return {false, "SET LAUNCH G TO ARM"};
        }
        if (!imu_calibrated) {
            return {false, "WAITING FOR IMU CALIBRATION"};
        }
        return {true, "WAITING FOR LAUNCH"};
    case settings::SessionTrigger::gps:
        if (!gnss_available) {
            return {false, "GPS UNAVAILABLE"};
        }
        return {true, "WAITING FOR THE LINE"};
    }
    return {true, ""};
}

bool launch_detected(const float longitudinal_g, const std::uint16_t launch_milli_g) noexcept
{
    if (launch_milli_g == 0) {
        return false;
    }
    return longitudinal_g * 1000.0F >= static_cast<float>(launch_milli_g);
}

}  // namespace track_timer::ui
