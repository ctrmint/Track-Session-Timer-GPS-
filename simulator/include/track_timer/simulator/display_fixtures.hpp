#pragma once

#include "track_timer/settings/settings.hpp"
#include "track_timer/ui/display_policy.hpp"

#include <cstdint>
#include <string_view>

namespace track_timer::simulator {

enum class DisplayFixtureId : std::uint8_t {
    live,
    day,
    night,
    dimmed,
    rotated,
};

struct DisplayFixture {
    ui::DisplayPolicyInput input{};
    settings::DeviceSettings settings{};
    bool override_settings{false};
};

[[nodiscard]] const char* display_fixture_name(DisplayFixtureId fixture) noexcept;
[[nodiscard]] bool parse_display_fixture(std::string_view name,
                                         DisplayFixtureId& fixture) noexcept;
[[nodiscard]] DisplayFixture make_display_fixture(
    DisplayFixtureId fixture, const settings::DeviceSettings& current) noexcept;

}  // namespace track_timer::simulator
