#include "track_timer/simulator/imu_fixtures.hpp"

#include <cmath>

namespace track_timer::simulator {

bool parse_imu_fixture(const std::string_view name, ImuFixtureId& fixture) noexcept
{
    if (name == "normal") {
        fixture = ImuFixtureId::normal;
    }
    else if (name == "calibration") {
        fixture = ImuFixtureId::calibration;
    }
    else if (name == "failure") {
        fixture = ImuFixtureId::failure;
    }
    else if (name == "partial") {
        fixture = ImuFixtureId::partial;
    }
    else if (name == "recovery") {
        fixture = ImuFixtureId::recovery;
    }
    else {
        return false;
    }
    return true;
}

const char* imu_fixture_name(const ImuFixtureId fixture) noexcept
{
    switch (fixture) {
    case ImuFixtureId::normal:
        return "normal";
    case ImuFixtureId::calibration:
        return "calibration";
    case ImuFixtureId::failure:
        return "failure";
    case ImuFixtureId::partial:
        return "partial";
    case ImuFixtureId::recovery:
        return "recovery";
    }
    return "normal";
}

ui::ImuMeterInput make_imu_fixture_input(const ImuFixtureId fixture,
                                         const std::uint64_t now_ms) noexcept
{
    ui::ImuMeterInput input{};
    input.now_ms = now_ms;
    input.sample.monotonic_us = static_cast<std::int64_t>(now_ms * 1'000U);

    if (fixture == ImuFixtureId::calibration) {
        input.calibrating = true;
        return input;
    }
    if (fixture == ImuFixtureId::failure ||
        (fixture == ImuFixtureId::recovery && now_ms < 600)) {
        return input;
    }

    const auto phase = static_cast<float>(now_ms % 2'000U) / 2'000.0F;
    constexpr float kPi = 3.14159265358979323846F;
    input.sample.acceleration_x_mps2 =
        std::sin(phase * 2.0F * kPi) * ui::kStandardGravityMps2 * 0.85F;
    input.sample.acceleration_y_mps2 =
        std::cos(phase * 2.0F * kPi) * ui::kStandardGravityMps2 * 0.65F;
    input.sample.acceleration_z_mps2 = ui::kStandardGravityMps2;
    input.sample.valid = true;
    input.sample_available = true;
    input.x_axis_valid = true;
    input.y_axis_valid = fixture != ImuFixtureId::partial;
    return input;
}

}  // namespace track_timer::simulator
