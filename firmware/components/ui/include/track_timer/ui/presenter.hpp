#pragma once

#include "track_timer/domain/contracts.hpp"

#include <array>
#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

enum class Readiness : std::uint8_t {
    unavailable,
    degraded,
    ready,
};

struct ReadySnapshot {
    std::array<char, 48> selected_track{};
    std::uint16_t session_duration_minutes{20};
    std::uint16_t rest_duration_minutes{20};
    domain::GnssHealth gnss_health{domain::GnssHealth::unavailable};
    Readiness storage{Readiness::unavailable};
    Readiness imu{Readiness::unavailable};
    bool logging_available{false};
    bool session_active{false};
};

struct ReadinessItem {
    std::array<char, 24> text{};
    std::uint32_t color_rgb{0xFFFFFF};
};

struct ReadyViewModel {
    std::array<char, 48> selected_track{};
    std::array<char, 24> session_duration{};
    std::array<char, 24> rest_duration{};
    std::array<char, 40> timing_mode{};
    ReadinessItem gnss{};
    ReadinessItem storage{};
    ReadinessItem imu{};
    ReadinessItem logging{};
    bool start_enabled{true};
    bool setup_enabled{true};
};

struct DeviceViewModel {
    std::array<char, 16> lap_label{};
    std::array<char, 32> current_lap{};
    std::array<char, 32> previous_lap{};
    std::array<char, 32> best_lap{};
    std::array<char, 32> session_remaining{};
    std::array<char, 24> session_status{};
    std::array<char, 16> gnss_status{};
    std::array<char, 16> logging_status{};
    domain::GnssHealth gnss_health{domain::GnssHealth::unavailable};
    std::uint32_t accent_rgb{0x202020};
    std::uint32_t accent_text_rgb{0xFFFFFF};
};

[[nodiscard]] DeviceViewModel present(const domain::UiSnapshot& snapshot) noexcept;
[[nodiscard]] ReadyViewModel present_ready(const ReadySnapshot& snapshot) noexcept;

static_assert(std::is_trivially_copyable_v<ReadySnapshot>);
static_assert(std::is_trivially_copyable_v<ReadyViewModel>);
static_assert(std::is_trivially_copyable_v<DeviceViewModel>);

}  // namespace track_timer::ui
