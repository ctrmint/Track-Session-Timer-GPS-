#include "track_timer/imu/calibration.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

namespace {

using namespace track_timer;

constexpr float kG = imu::kStandardGravityMps2;

board::ImuSample sample_of(float x, float y, float z)
{
    board::ImuSample sample{};
    sample.acceleration_x_mps2 = x;
    sample.acceleration_y_mps2 = y;
    sample.acceleration_z_mps2 = z;
    sample.valid = true;
    return sample;
}

bool near(float actual, float expected, float tolerance = 0.02F)
{
    return std::fabs(actual - expected) <= tolerance;
}

void settle(imu::GravityCalibration& calibration, const board::ImuSample& at_rest)
{
    for (std::size_t index = 0; index < imu::kRestSamplesRequired + 2; ++index) {
        (void)calibration.update(at_rest);
    }
}

constexpr std::int64_t kStepUs = 50'000;  // the 20 Hz service tick the device runs at

board::ImuSample sample_at(float x, float y, float z, std::int64_t us, float gx = 0.0F,
                           float gy = 0.0F, float gz = 0.0F)
{
    auto sample = sample_of(x, y, z);
    sample.monotonic_us = us;
    sample.angular_rate_x_dps = gx;
    sample.angular_rate_y_dps = gy;
    sample.angular_rate_z_dps = gz;
    return sample;
}

// Settles with timestamps, so attitude tracking has an interval to work over. Returns the
// timestamp to carry on from.
std::int64_t settle_at(imu::GravityCalibration& calibration, float x, float y, float z,
                       float gx = 0.0F, float gy = 0.0F, float gz = 0.0F)
{
    std::int64_t us = kStepUs;
    for (std::size_t index = 0; index < imu::kRestSamplesRequired + 2; ++index) {
        (void)calibration.update(sample_at(x, y, z, us, gx, gy, gz));
        us += kStepUs;
    }
    return us;
}

// The device does not sit level on a dashboard, so gravity lands across several axes.
// This is the vector the real board reported at rest, magnitude 1.077 g.
const auto kRealResting = sample_of(8.21F, 6.62F, -0.56F);

void calibration_becomes_ready_only_after_enough_still_samples()
{
    imu::GravityCalibration calibration;
    assert(calibration.state() == imu::CalibrationState::collecting);
    for (std::size_t index = 0; index < imu::kRestSamplesRequired - 1; ++index) {
        (void)calibration.update(kRealResting);
    }
    assert(calibration.state() == imu::CalibrationState::collecting);
    (void)calibration.update(kRealResting);
    assert(calibration.state() == imu::CalibrationState::ready);
}

// The whole point: at rest the dot must sit at the centre, whatever the mounting angle.
void at_rest_the_reading_is_zero_on_both_axes()
{
    imu::GravityCalibration calibration;
    settle(calibration, kRealResting);

    const auto resolved = calibration.resolve(kRealResting);
    assert(resolved.valid);
    assert(near(resolved.lateral_g, 0.0F));
    assert(near(resolved.longitudinal_g, 0.0F));
}

// Uncalibrated the board reads 1.077 g at rest; the correction must take that out.
void the_scale_error_is_measured_and_removed()
{
    imu::GravityCalibration calibration;
    settle(calibration, kRealResting);

    const auto measured = std::sqrt(8.21F * 8.21F + 6.62F * 6.62F + 0.56F * 0.56F);
    assert(near(measured / kG, 1.077F, 0.005F));
    // Scale brings the measured magnitude back to exactly 1 g.
    assert(near(calibration.scale_correction() * measured, kG, 0.01F));
}

// Any mounting angle must centre, not just the one the board happened to be at.
void any_mounting_angle_still_centres()
{
    const board::ImuSample orientations[] = {
        sample_of(0.0F, 0.0F, kG),        // flat, screen up
        sample_of(0.0F, kG, 0.0F),        // upright
        sample_of(kG, 0.0F, 0.0F),        // on its side
        sample_of(4.9F, 4.9F, 6.93F),     // tilted every which way
        sample_of(-5.66F, 8.0F, 0.0F),    // rolled
    };
    for (const auto& at_rest : orientations) {
        imu::GravityCalibration calibration;
        settle(calibration, at_rest);
        assert(calibration.state() == imu::CalibrationState::ready);
        const auto resolved = calibration.resolve(at_rest);
        assert(resolved.valid);
        assert(near(resolved.lateral_g, 0.0F, 0.03F));
        assert(near(resolved.longitudinal_g, 0.0F, 0.03F));
    }
}

// Upright, screen facing the driver: braking pushes the sensor toward the driver, which
// is the +Z direction, and must read as braking rather than acceleration.
void braking_and_acceleration_land_on_the_longitudinal_axis()
{
    imu::GravityCalibration calibration{imu::MountFacing::screen_to_driver};
    const auto upright = sample_of(0.0F, kG, 0.0F);
    settle(calibration, upright);

    const auto braking = calibration.resolve(sample_of(0.0F, kG, 0.5F * kG));
    assert(braking.valid);
    assert(near(braking.longitudinal_g, -0.5F, 0.03F));
    assert(near(braking.lateral_g, 0.0F, 0.03F));

    const auto accelerating = calibration.resolve(sample_of(0.0F, kG, -0.5F * kG));
    assert(near(accelerating.longitudinal_g, 0.5F, 0.03F));
}

void cornering_lands_on_the_lateral_axis()
{
    imu::GravityCalibration calibration;
    settle(calibration, sample_of(0.0F, kG, 0.0F));

    const auto sideways = calibration.resolve(sample_of(0.8F * kG, kG, 0.0F));
    assert(sideways.valid);
    assert(near(std::fabs(sideways.lateral_g), 0.8F, 0.03F));
    assert(near(sideways.longitudinal_g, 0.0F, 0.03F));
}

// Facing changes which way is forward, so the longitudinal sign must flip with it.
void mount_facing_flips_the_longitudinal_sign()
{
    const auto upright = sample_of(0.0F, kG, 0.0F);
    const auto pushed = sample_of(0.0F, kG, 0.5F * kG);

    imu::GravityCalibration to_driver{imu::MountFacing::screen_to_driver};
    settle(to_driver, upright);
    imu::GravityCalibration forward{imu::MountFacing::screen_forward};
    settle(forward, upright);

    const auto a = to_driver.resolve(pushed).longitudinal_g;
    const auto b = forward.resolve(pushed).longitudinal_g;
    assert(near(a, -b, 0.03F));
}

// Movement must not be mistaken for rest, or the zero point learns a cornering load.
void movement_prevents_calibration()
{
    imu::GravityCalibration calibration;
    for (std::size_t index = 0; index < imu::kRestSamplesRequired * 3; ++index) {
        // Alternating well beyond 1 g: unmistakably not at rest.
        (void)calibration.update(index % 2 == 0 ? sample_of(0.0F, kG, 0.0F)
                                                : sample_of(0.0F, 2.0F * kG, 0.0F));
    }
    assert(calibration.state() == imu::CalibrationState::collecting);
    assert(!calibration.resolve(sample_of(0.0F, kG, 0.0F)).valid);
}

// Once learned, the reference must survive driving, or the zero drifts mid-session.
void a_learned_reference_survives_movement()
{
    imu::GravityCalibration calibration;
    const auto upright = sample_of(0.0F, kG, 0.0F);
    settle(calibration, upright);
    const auto learned = calibration.gravity();

    for (std::size_t index = 0; index < 200; ++index) {
        (void)calibration.update(sample_of(0.9F * kG, kG, 0.4F * kG));
    }
    assert(calibration.state() == imu::CalibrationState::ready);
    assert(near(calibration.gravity().y, learned.y, 0.001F));
}

void an_invalid_sample_is_refused()
{
    imu::GravityCalibration calibration;
    settle(calibration, sample_of(0.0F, kG, 0.0F));
    board::ImuSample bad{};
    bad.valid = false;
    assert(!calibration.resolve(bad).valid);
}

void restart_clears_the_reference()
{
    imu::GravityCalibration calibration;
    settle(calibration, kRealResting);
    assert(calibration.state() == imu::CalibrationState::ready);
    calibration.restart();
    assert(calibration.state() == imu::CalibrationState::collecting);
    assert(!calibration.resolve(kRealResting).valid);
    assert(std::strcmp(imu::calibration_state_name(imu::CalibrationState::ready),
                       "ready") == 0);
}

// Right-handed vehicle axes: forward x up = right. An accelerometer at rest measures
// specific force, which points up, so treating the rest reading as "down" inverts this and
// reports a right-hand corner as a left-hand one.
void positive_lateral_is_to_the_right()
{
    imu::GravityCalibration calibration{imu::MountFacing::screen_to_driver};
    (void)settle_at(calibration, 0.0F, kG, 0.0F);

    // Up is +Y and forward is -Z, so right is +X. Turning right accelerates the car to the
    // right, and specific force gains a +X component to match.
    const auto resolved = calibration.resolve(sample_of(0.5F * kG, kG, 0.0F));
    assert(resolved.valid);
    assert(near(resolved.lateral_g, 0.5F, 0.03F));
}

// Kerbs and compressions are a different kind of event from anything the tyres do in plan,
// so the vertical axis is reported rather than discarded.
void positive_vertical_is_upward()
{
    imu::GravityCalibration calibration;
    (void)settle_at(calibration, 0.0F, kG, 0.0F);
    assert(near(calibration.resolve(sample_of(0.0F, kG, 0.0F)).vertical_g, 0.0F));

    const auto compression = calibration.resolve(sample_of(0.0F, 1.3F * kG, 0.0F));
    assert(near(compression.vertical_g, 0.3F, 0.03F));
    assert(near(compression.lateral_g, 0.0F, 0.03F));
    assert(near(compression.longitudinal_g, 0.0F, 0.03F));
}

// Rolling the unit is not acceleration. A reference frozen at rest turns every later change
// of tilt into false G at sin(angle); the gyroscope measures that rotation, so the reference
// turns with it instead of being left behind.
void a_tilt_is_followed_rather_than_read_as_g()
{
    constexpr float kRateDps = 40.0F;
    constexpr int kSteps = 10;  // half a second, so twenty degrees of roll

    const auto roll = [](bool report_rate) {
        imu::GravityCalibration calibration;
        auto us = settle_at(calibration, 0.0F, kG, 0.0F);
        board::ImuSample last{};
        for (int step = 1; step <= kSteps; ++step) {
            const auto degrees = kRateDps * static_cast<float>(step) *
                                 (static_cast<float>(kStepUs) / 1.0e6F);
            const auto radians = degrees * 3.14159265F / 180.0F;
            last = sample_at(kG * std::sin(radians), kG * std::cos(radians), 0.0F, us,
                             0.0F, 0.0F, report_rate ? kRateDps : 0.0F);
            (void)calibration.update(last);
            us += kStepUs;
        }
        return calibration.resolve(last);
    };

    const auto tracked = roll(true);
    assert(tracked.valid);
    assert(near(tracked.lateral_g, 0.0F, 0.03F));
    assert(near(tracked.longitudinal_g, 0.0F, 0.03F));

    // The same tilt with the gyroscope silent is the error this exists to remove: twenty
    // degrees is sin(20) = 0.34 g of cornering load that the car never pulled.
    assert(std::fabs(roll(false).lateral_g) > 0.20F);
}

// The mirror image of the tilt case: a long constant-radius corner is real acceleration and
// must not be learned as the new level, however steady it looks.
void a_sustained_corner_is_not_absorbed()
{
    imu::GravityCalibration calibration;
    auto us = settle_at(calibration, 0.0F, kG, 0.0F);
    for (int step = 0; step < 100; ++step) {  // five seconds
        (void)calibration.update(sample_at(0.6F * kG, kG, 0.0F, us));
        us += kStepUs;
    }

    const auto resolved = calibration.resolve(sample_at(0.6F * kG, kG, 0.0F, us));
    assert(near(resolved.lateral_g, 0.6F, 0.03F));
    // The accelerometer is not measuring gravity alone here, so it must not be believed.
    assert(calibration.coasting());
}

// Whatever the gyroscope reports while the device is still is its zero-rate offset.
// Integrating that unremoved is a ramp straight into false G.
void the_gyro_zero_rate_offset_is_learned_and_removed()
{
    imu::GravityCalibration calibration;
    auto us = settle_at(calibration, 0.0F, kG, 0.0F, 2.0F, -1.0F, 3.0F);

    const auto bias = calibration.gyro_bias();
    assert(near(bias.x, 2.0F, 0.1F));
    assert(near(bias.y, -1.0F, 0.1F));
    assert(near(bias.z, 3.0F, 0.1F));

    // Held still and still reporting that offset, the estimate must not walk. Unremoved,
    // 3 dps for a second is three degrees, which is 0.05 g of invented cornering.
    board::ImuSample last{};
    for (int step = 0; step < 20; ++step) {
        last = sample_at(0.0F, kG, 0.0F, us, 2.0F, -1.0F, 3.0F);
        (void)calibration.update(last);
        us += kStepUs;
    }
    const auto resolved = calibration.resolve(last);
    assert(near(resolved.lateral_g, 0.0F, 0.02F));
    assert(near(resolved.longitudinal_g, 0.0F, 0.02F));
}

// A device being turned is not at rest, whatever the force on it looks like. The gate tests
// the change in rate rather than the rate itself, so a unit with a large zero-rate offset
// still calibrates.
void a_changing_turn_rate_prevents_calibration()
{
    imu::GravityCalibration calibration;
    std::int64_t us = kStepUs;
    for (std::size_t index = 0; index < imu::kRestSamplesRequired * 3; ++index) {
        const auto rate = (index % 2 == 0) ? 0.0F : 4.0F * imu::kRestRateJitterDps;
        (void)calibration.update(sample_at(0.0F, kG, 0.0F, us, 0.0F, 0.0F, rate));
        us += kStepUs;
    }
    assert(calibration.state() == imu::CalibrationState::collecting);
}

}  // namespace

int main()
{
    calibration_becomes_ready_only_after_enough_still_samples();
    at_rest_the_reading_is_zero_on_both_axes();
    the_scale_error_is_measured_and_removed();
    any_mounting_angle_still_centres();
    braking_and_acceleration_land_on_the_longitudinal_axis();
    cornering_lands_on_the_lateral_axis();
    mount_facing_flips_the_longitudinal_sign();
    movement_prevents_calibration();
    a_learned_reference_survives_movement();
    an_invalid_sample_is_refused();
    restart_clears_the_reference();
    positive_lateral_is_to_the_right();
    positive_vertical_is_upward();
    a_tilt_is_followed_rather_than_read_as_g();
    a_sustained_corner_is_not_absorbed();
    the_gyro_zero_rate_offset_is_learned_and_removed();
    a_changing_turn_rate_prevents_calibration();

    std::cout << "Gravity auto-calibration: centring at any mounting angle, scale "
                 "correction, vehicle-axis resolution, and gyro-tracked attitude passed\n";
    return 0;
}
