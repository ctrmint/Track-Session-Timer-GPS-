#pragma once

#include "track_timer/settings/settings.hpp"

#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

// A single user-facing Mode, derived from settings that already exist rather than
// introduced as a third parallel concept:
//
//   Track Day  operating_mode = timer,   trackday_mode_enabled = true
//   Race       operating_mode = timer,   trackday_mode_enabled = false
//   G-Only     operating_mode = g_meter
//
// Deriving rather than adding a field means the persisted settings format is unchanged,
// so no schema version bump and no migration, and the existing tested Trackday Mode
// behaviour in ActiveSessionController is reused as-is.
enum class DeviceMode : std::uint8_t {
    track_day,
    race,
    g_only,
};
inline constexpr std::size_t kDeviceModeCount = 3;

// What each mode is permitted to show while a session is running.
struct ModeVisibility {
    bool session_timer{true};
    bool lap_times{false};
    bool lap_delta{false};
    bool g_meter{false};
};

// Track Day withholds live lap times deliberately. Many track days run under regulations
// that prohibit timing on circuit, so this is a compliance rule, not a preference. Laps
// are still recorded and remain available in Review once the session has ended.
[[nodiscard]] ModeVisibility visibility_for(DeviceMode mode) noexcept;

[[nodiscard]] DeviceMode mode_from_settings(const settings::DeviceSettings& settings) noexcept;
void apply_mode(DeviceMode mode, settings::DeviceSettings& settings) noexcept;

[[nodiscard]] const char* device_mode_name(DeviceMode mode) noexcept;
[[nodiscard]] const char* device_mode_label(DeviceMode mode) noexcept;
[[nodiscard]] const char* device_mode_summary(DeviceMode mode) noexcept;

static_assert(std::is_trivially_copyable_v<ModeVisibility>);

}  // namespace track_timer::ui
