#pragma once

#include "track_timer/ui/imu_meter.hpp"

#include <cstdint>
#include <string_view>

namespace track_timer::simulator {

enum class ImuFixtureId : std::uint8_t {
    normal,
    calibration,
    failure,
    partial,
    recovery,
};

[[nodiscard]] bool parse_imu_fixture(std::string_view name, ImuFixtureId& fixture) noexcept;
[[nodiscard]] const char* imu_fixture_name(ImuFixtureId fixture) noexcept;
[[nodiscard]] ui::ImuMeterInput make_imu_fixture_input(ImuFixtureId fixture,
                                                       std::uint64_t now_ms) noexcept;

}  // namespace track_timer::simulator
