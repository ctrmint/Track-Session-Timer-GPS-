#pragma once

#include "track_timer/board/platform.hpp"
#include "track_timer/settings/settings.hpp"

#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

inline constexpr std::uint64_t kReadyAutoDimDelayMs = 60'000;
inline constexpr std::uint64_t kAmoledShiftIntervalMs = 30'000;
inline constexpr std::int8_t kMaximumAmoledShiftPx = 4;
inline constexpr std::uint8_t kDimmedBrightnessPercent = 25;

enum class BrightnessProfile : std::uint8_t {
    day,
    night,
};

enum class DisplayContext : std::uint8_t {
    ready,
    active,
    other,
};

struct DisplayPolicyInput {
    settings::DeviceSettings settings{};
    std::uint64_t now_ms{0};
    BrightnessProfile brightness_profile{BrightnessProfile::day};
    board::DisplayOrientation sensed_orientation{board::DisplayOrientation::degrees_0};
    DisplayContext context{DisplayContext::other};
    bool sensed_orientation_available{false};
    bool stationary{true};
    bool user_activity{false};
    bool settings_preview{false};
};

struct DisplayPolicySnapshot {
    board::DisplayCommand command{};
    BrightnessProfile brightness_profile{BrightnessProfile::day};
    bool orientation_deferred{false};
    bool settings_preview{false};
};

class DisplayPolicyController {
  public:
    [[nodiscard]] const DisplayPolicySnapshot& update(
        const DisplayPolicyInput& input) noexcept;
    [[nodiscard]] const DisplayPolicySnapshot& snapshot() const noexcept;

  private:
    [[nodiscard]] board::DisplayOrientation resolve_orientation(
        const DisplayPolicyInput& input) const noexcept;
    void apply_idle_policy(const DisplayPolicyInput& input) noexcept;

    DisplayPolicySnapshot snapshot_{};
    std::uint64_t idle_started_ms_{0};
    bool idle_started_{false};
    bool active_{false};
};

[[nodiscard]] const char* brightness_profile_name(BrightnessProfile profile) noexcept;
[[nodiscard]] const char* display_orientation_name(
    board::DisplayOrientation orientation) noexcept;
[[nodiscard]] std::uint16_t display_orientation_degrees(
    board::DisplayOrientation orientation) noexcept;

static_assert(std::is_trivially_copyable_v<DisplayPolicyInput>);
static_assert(std::is_trivially_copyable_v<DisplayPolicySnapshot>);

}  // namespace track_timer::ui
