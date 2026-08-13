#include "track_timer/simulator/display_fixtures.hpp"

#include <array>

namespace track_timer::simulator {

const char* display_fixture_name(const DisplayFixtureId fixture) noexcept
{
    switch (fixture) {
    case DisplayFixtureId::live:
        return "live";
    case DisplayFixtureId::day:
        return "day";
    case DisplayFixtureId::night:
        return "night";
    case DisplayFixtureId::dimmed:
        return "dimmed";
    case DisplayFixtureId::rotated:
        return "rotated";
    }
    return "live";
}

bool parse_display_fixture(const std::string_view name,
                           DisplayFixtureId& fixture) noexcept
{
    constexpr std::array fixtures{
        DisplayFixtureId::live,
        DisplayFixtureId::day,
        DisplayFixtureId::night,
        DisplayFixtureId::dimmed,
        DisplayFixtureId::rotated,
    };
    for (const auto candidate : fixtures) {
        if (name == display_fixture_name(candidate)) {
            fixture = candidate;
            return true;
        }
    }
    return false;
}

DisplayFixture make_display_fixture(const DisplayFixtureId fixture,
                                    const settings::DeviceSettings& current) noexcept
{
    DisplayFixture result{};
    result.settings = current;
    if (fixture == DisplayFixtureId::live) {
        return result;
    }

    result.override_settings = true;
    result.settings.day_brightness_percent = 100;
    result.settings.night_brightness_percent = 50;
    result.settings.orientation = settings::OrientationMode::fixed_0;
    result.settings.auto_dim_enabled = false;
    result.input.brightness_profile = ui::BrightnessProfile::day;
    result.input.stationary = true;

    switch (fixture) {
    case DisplayFixtureId::live:
        break;
    case DisplayFixtureId::day:
        break;
    case DisplayFixtureId::night:
        result.input.brightness_profile = ui::BrightnessProfile::night;
        break;
    case DisplayFixtureId::dimmed:
        result.settings.auto_dim_enabled = true;
        break;
    case DisplayFixtureId::rotated:
        result.settings.orientation = settings::OrientationMode::automatic;
        result.input.sensed_orientation_available = true;
        result.input.sensed_orientation = board::DisplayOrientation::degrees_90;
        break;
    }
    return result;
}

}  // namespace track_timer::simulator
