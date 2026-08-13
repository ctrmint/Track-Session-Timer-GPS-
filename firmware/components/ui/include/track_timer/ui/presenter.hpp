#pragma once

#include "track_timer/domain/contracts.hpp"

#include <array>
#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

struct DeviceViewModel {
    std::array<char, 16> lap_label{};
    std::array<char, 32> current_lap{};
    std::array<char, 32> previous_lap{};
    std::array<char, 32> best_lap{};
    std::array<char, 32> session_remaining{};
    std::array<char, 16> gnss_status{};
    std::array<char, 16> logging_status{};
    domain::GnssHealth gnss_health{domain::GnssHealth::unavailable};
    std::uint32_t accent_rgb{0x202020};
    std::uint32_t accent_text_rgb{0xFFFFFF};
};

[[nodiscard]] DeviceViewModel present(const domain::UiSnapshot& snapshot) noexcept;

static_assert(std::is_trivially_copyable_v<DeviceViewModel>);

}  // namespace track_timer::ui
