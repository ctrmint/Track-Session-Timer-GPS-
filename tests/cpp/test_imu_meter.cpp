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

// Vertical arrives on the sample's z component and, unlike the horizontal pair, does not
// rotate with the display: up is up however the unit is mounted.
track_timer::ui::ImuMeterInput vertical_sample(const float vertical_g,
                                               const std::uint64_t now_ms)
{
    auto input = sample(0.0F, 0.0F, now_ms);
    input.sample.acceleration_z_mps2 = vertical_g * track_timer::ui::kStandardGravityMps2;
    input.z_axis_valid = true;
    return input;
}

}  // namespace

// #136 asked for "a trace of the last few seconds". That is a property of the span, so the
// span is what gets asserted: a capacity on its own could mean any duration, which is how
// 24 points came to be read as a few seconds when it was one and a fifth of one.
static_assert(track_timer::ui::kImuTrailSpanMs >= 2'000,
              "the trace has to cover a corner rather than a fraction of one");
static_assert(track_timer::ui::kImuTrailCapacity *
                      track_timer::ui::kImuTrailSamplePeriodUs / 1'000 ==
                  track_timer::ui::kImuTrailSpanMs,
              "the capacity has to hold exactly the span it claims to");

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

    // Overfilled relative to whatever the capacity currently is, not to a literal: the
    // capacity is derived from the trace span and the display cadence now, so a test
    // written against the number it happens to produce breaks every time either changes
    // and tells you nothing about the ring buffer either way. Sixteen past full is enough
    // to prove the oldest points are dropped rather than the newest refused.
    constexpr std::uint64_t kOverfill = ui::kImuTrailCapacity + 16;
    for (std::uint64_t index = 0; index < kOverfill; ++index) {
        input = sample(static_cast<float>(index) / 100.0F, 0.1F, 30 + index);
        meter.update(input, false);
    }
    assert(meter.snapshot().trail_count == ui::kImuTrailCapacity);
    assert(meter.trail_point(ui::kImuTrailCapacity).lateral_g == 0.0F);
    assert(near(meter.trail_point(ui::kImuTrailCapacity - 1).lateral_g,
                static_cast<float>(kOverfill - 1) / 100.0F));

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

    // A kerb throws the car up and a compression loads it down. Recording only a magnitude
    // would lose which happened, so the two are held apart.
    ui::ImuMeterController vertical;
    (void)vertical.update(vertical_sample(0.8F, 0), false);
    (void)vertical.update(vertical_sample(-1.3F, 100), false);
    (void)vertical.update(vertical_sample(0.2F, 200), false);
    assert(near(vertical.snapshot().peaks.up_g, 0.8F));
    assert(near(vertical.snapshot().peaks.down_g, 1.3F));
    assert(near(vertical.snapshot().vertical_g, 0.2F));
    assert(vertical.snapshot().vertical_valid);

    // Without the axis the peaks must stay put rather than collapsing to zero.
    (void)vertical.update(sample(0.0F, 0.0F, 300), false);
    assert(!vertical.snapshot().vertical_valid);
    assert(near(vertical.snapshot().peaks.up_g, 0.8F));
    assert(near(vertical.snapshot().peaks.down_g, 1.3F));

    // Peaks belong to a session. Starting one clears what came before, so a summary reports
    // what the car did on track rather than everything since boot - including whatever
    // happened carrying the device to the car.
    ui::ImuMeterController scoped;
    (void)scoped.update(sample(1.5F, 0.0F, 0), false);
    (void)scoped.update(vertical_sample(2.0F, 100), false);
    assert(scoped.snapshot().peaks.left_g > 1.0F || scoped.snapshot().peaks.right_g > 1.0F);
    assert(near(scoped.snapshot().peaks.up_g, 2.0F));

    (void)scoped.update(sample(0.4F, 0.0F, 200), true);   // session starts
    assert(near(scoped.snapshot().peaks.up_g, 0.0F));
    assert(!scoped.snapshot().reset_allowed);             // and cannot be cleared by hand

    (void)scoped.update(vertical_sample(0.5F, 300), true);
    assert(near(scoped.snapshot().peaks.up_g, 0.5F));

    // Ending the session leaves the peaks standing, because that is when they are read.
    (void)scoped.update(sample(0.0F, 0.0F, 400), false);
    assert(near(scoped.snapshot().peaks.up_g, 0.5F));
    assert(scoped.snapshot().reset_allowed);

    // #162. The radar clamps to 2 g so a dot stays on the dial, and that clamp used to be
    // applied before the peaks were taken - so a session's recorded peak was bounded by
    // what the display could draw rather than by what the car did. A hand-shaken session
    // recorded 2577 milli-g, which is both horizontal axes pinned at the limit rather than
    // a measurement.
    {
        ui::ImuMeterController recorder{};
        auto hard = sample(2.6F, 1.4F, 10);
        auto recorded = recorder.update(hard, false);

        // Measured, not bounded by the dial.
        assert(near(recorded.peaks.right_g, 2.6F));
        assert(near(recorded.peaks.acceleration_g, 1.4F));
        assert(!recorded.peaks.truncated);

        // The drawn value still is bounded, because the dot has to stay on the display.
        assert(near(recorded.current.lateral_g, ui::kImuDisplayLimitG));
        assert(near(recorded.current.longitudinal_g, 1.4F));

        // And the total comes from the measurement rather than from two clamped axes.
        assert(near(recorded.peaks.total_g, std::sqrt(2.6F * 2.6F + 1.4F * 1.4F)));
        assert(recorded.peaks.total_g > std::sqrt(2.0F * 2.0F + 1.4F * 1.4F));
    }

    // Vertical is the axis where this actually bites. A car on road tyres never reaches
    // 2 g sideways, but a kerb strike passes it easily - and used to be filed as exactly
    // 2.00 with nothing to say it had been truncated.
    {
        ui::ImuMeterController recorder{};
        auto kerb = vertical_sample(3.4F, 10);
        auto recorded = recorder.update(kerb, false);
        assert(near(recorded.peaks.up_g, 3.4F));
        assert(!recorded.peaks.truncated);
        // The dial still shows what it can draw.
        assert(near(recorded.vertical_g, ui::kImuDisplayLimitG));

        auto compression = vertical_sample(-2.9F, 20);
        recorded = recorder.update(compression, false);
        assert(near(recorded.peaks.down_g, 2.9F));
    }

    // A bound remains, so there is still a number that could be a clamp wearing a
    // measurement's clothes. Past 10 g is a sensor fault rather than a lap - the summary
    // validator refuses the record beyond it - so it is bounded and says it was.
    {
        ui::ImuMeterController recorder{};
        auto fault = sample(14.0F, 0.2F, 10);
        auto recorded = recorder.update(fault, false);
        assert(near(recorded.peaks.right_g, ui::kImuRecordedLimitG));
        // The flag is the difference between a bound and a lie.
        assert(recorded.peaks.truncated);
    }

    // An ordinary session never touches either bound and is never marked.
    {
        ui::ImuMeterController recorder{};
        auto ordinary = sample(1.1F, 0.9F, 10);
        const auto recorded = recorder.update(ordinary, false);
        assert(near(recorded.peaks.right_g, 1.1F));
        assert(!recorded.peaks.truncated);
    }

    std::cout << "G-meter peaks recorded as measured rather than bounded by the dial, "
                 "orientation, reset guard, IMU states, and "
                 "session-scoped vertical peaks passed\n";
    return 0;
}
