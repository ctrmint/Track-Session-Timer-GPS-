#pragma once

#include "track_timer/settings/settings.hpp"

#include <cstddef>
#include <cstdint>

namespace track_timer::ui {

inline constexpr std::size_t kSessionTriggerCount = 3;

// Why a trigger owns pit-exit auto-start rather than sitting beside it:
//
// `pit_exit_auto_start_enabled` already meant "start the session when the car leaves the
// pits", with tested automation behind it. GPS is the same intent, so the trigger writes
// that setting and it leaves the device settings list. Offering one behaviour in two
// places invites the two disagreeing, which is the rule Mode established.
void apply_trigger(settings::SessionTrigger trigger,
                   settings::DeviceSettings& settings) noexcept;
[[nodiscard]] settings::SessionTrigger trigger_from_settings(
    const settings::DeviceSettings& settings) noexcept;

[[nodiscard]] const char* session_trigger_name(settings::SessionTrigger trigger) noexcept;
[[nodiscard]] const char* session_trigger_label(settings::SessionTrigger trigger) noexcept;
[[nodiscard]] const char* session_trigger_summary(settings::SessionTrigger trigger) noexcept;

// Whether a trigger can arm at all, and what to say when it cannot. A trigger the driver
// has selected but that cannot fire is worse than no trigger: the device simply sits
// there. Both cases are reportable rather than silent.
struct TriggerReadiness {
    bool can_arm{true};
    const char* detail{""};
};

// `launch_milli_g` is the LAUNCH setting; zero means off, which with the IMU trigger is a
// session that can never start. `gnss_available` is false until a receiver exists.
[[nodiscard]] TriggerReadiness trigger_readiness(settings::SessionTrigger trigger,
                                                 std::uint16_t launch_milli_g,
                                                 bool imu_calibrated,
                                                 bool gnss_available) noexcept;

// True when forward acceleration has reached the configured launch threshold. Forward
// only: a launch is acceleration down the road, so braking, cornering and kerbs cannot
// start a session. Never true at a zero threshold, which means off.
[[nodiscard]] bool launch_detected(float longitudinal_g, std::uint16_t launch_milli_g) noexcept;

}  // namespace track_timer::ui
