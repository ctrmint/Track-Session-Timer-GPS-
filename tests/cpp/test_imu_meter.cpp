#include "track_timer/simulator/imu_fixtures.hpp"
#include "track_timer/ui/imu_meter.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {

bool near(const float actual, const float expected, const float tolerance = 0.001F)
{
    return std::fabs(actual - expected) <= tolerance;
}

track_timer::ui::ImuMeterInput sample(const float lateral_g, const float longitudinal_g,
                                      const std::uint64_t now_ms)
{
    track_timer::ui::ImuMeterInput input{};
    input.sample.acceleration_x_mps2 = lateral_g * track_timer::ui::kStandardGravityMps2;
    input.sample.acceleration_y_mps2 = longitudinal_g * track_timer::ui::kStandardGravityMps2;
    input.sample.valid = true;
    input.sample_available = true;
    input.x_axis_valid = true;
    input.y_axis_valid = true;
    input.now_ms = now_ms;
    return input;
}

}  // namespace

int main()
{
    using namespace track_timer;

    ui::ImuMeterController meter{};
    auto input = sample(0.5F, 0.8F, 10);
    auto snapshot = meter.update(input, false);
    assert(snapshot.state == ui::ImuMeterState::ready);
    assert(near(snapshot.current.lateral_g, 0.5F));
    assert(near(snapshot.current.longitudinal_g, 0.8F));
    assert(near(snapshot.peaks.acceleration_g, 0.8F));
    assert(near(snapshot.peaks.right_g, 0.5F));
    assert(near(snapshot.peaks.total_g, std::sqrt(0.89F)));

    input = sample(-0.9F, -1.1F, 20);
    snapshot = meter.update(input, false);
    assert(near(snapshot.peaks.braking_g, 1.1F));
    assert(near(snapshot.peaks.left_g, 0.9F));

    for (std::uint64_t index = 0; index < 40; ++index) {
        input = sample(static_cast<float>(index) / 100.0F, 0.1F, 30 + index);
        meter.update(input, false);
    }
    assert(meter.snapshot().trail_count == ui::kImuTrailCapacity);
    assert(meter.trail_point(ui::kImuTrailCapacity).lateral_g == 0.0F);
    assert(near(meter.trail_point(ui::kImuTrailCapacity - 1).lateral_g, 0.39F));

    input = sample(0.1F, 0.2F, 80);
    snapshot = meter.update(input, true);
    assert(snapshot.trail_count == 1);
    assert(near(snapshot.peaks.acceleration_g, 0.2F));
    assert(near(snapshot.peaks.right_g, 0.1F));
    snapshot = meter.update(input, false);
    assert(snapshot.trail_count == 2);
    assert(near(snapshot.peaks.acceleration_g, 0.2F));

    ui::ImuMeterController rotated{};
    input = sample(1.0F, 0.5F, 100);
    input.orientation = board::DisplayOrientation::degrees_90;
    snapshot = rotated.update(input, false);
    assert(near(snapshot.current.lateral_g, -0.5F));
    assert(near(snapshot.current.longitudinal_g, 1.0F));
    assert(snapshot.orientation == board::DisplayOrientation::degrees_90);

    ui::ImuMeterController upside_down{};
    input.orientation = board::DisplayOrientation::degrees_180;
    snapshot = upside_down.update(input, false);
    assert(near(snapshot.current.lateral_g, -1.0F));
    assert(near(snapshot.current.longitudinal_g, -0.5F));

    ui::ImuMeterController rotated_left{};
    input.orientation = board::DisplayOrientation::degrees_270;
    snapshot = rotated_left.update(input, false);
    assert(near(snapshot.current.lateral_g, 0.5F));
    assert(near(snapshot.current.longitudinal_g, -1.0F));

    auto partial = simulator::make_imu_fixture_input(simulator::ImuFixtureId::partial, 200);
    snapshot = rotated.update(partial, false);
    assert(snapshot.state == ui::ImuMeterState::partial);
    assert(snapshot.current.lateral_valid);
    assert(!snapshot.current.longitudinal_valid);

    const auto active_sample = sample(0.2F, 0.3F, 250);
    snapshot = rotated.update(active_sample, true);
    assert(snapshot.trail_count == 1);
    auto failed = simulator::make_imu_fixture_input(simulator::ImuFixtureId::failure, 300);
    snapshot = rotated.update(failed, true);
    assert(snapshot.state == ui::ImuMeterState::unavailable);
    assert(!snapshot.reset_allowed);
    assert(!rotated.reset(true));
    assert(rotated.snapshot().trail_count > 0);

    auto recovered = simulator::make_imu_fixture_input(simulator::ImuFixtureId::recovery, 600);
    snapshot = rotated.update(recovered, false);
    assert(snapshot.state == ui::ImuMeterState::recovered);
    recovered.now_ms = 2'101;
    snapshot = rotated.update(recovered, false);
    assert(snapshot.state == ui::ImuMeterState::ready);

    auto calibrating = simulator::make_imu_fixture_input(
        simulator::ImuFixtureId::calibration, 3'000);
    snapshot = rotated.update(calibrating, false);
    assert(snapshot.state == ui::ImuMeterState::calibrating);

    assert(rotated.reset(false));
    assert(rotated.snapshot().trail_count == 0);
    assert(near(rotated.snapshot().peaks.total_g, 0.0F));

    std::cout << "Bounded G-meter peaks, orientation, reset guard, and IMU states passed\n";
    return 0;
}
