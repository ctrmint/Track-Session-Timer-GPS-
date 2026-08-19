#include "track_timer/imu/calibration.hpp"

#include <cmath>

namespace track_timer::imu {
namespace {

inline constexpr float kRadiansPerDegree = 3.14159265358979F / 180.0F;

[[nodiscard]] float magnitude(const Vector3& v) noexcept
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

[[nodiscard]] Vector3 scaled(const Vector3& v, const float factor) noexcept
{
    return {v.x * factor, v.y * factor, v.z * factor};
}

[[nodiscard]] Vector3 subtract(const Vector3& a, const Vector3& b) noexcept
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] Vector3 cross(const Vector3& a, const Vector3& b) noexcept
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

[[nodiscard]] float dot(const Vector3& a, const Vector3& b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] Vector3 normalised(const Vector3& v) noexcept
{
    const auto length = magnitude(v);
    return length > 1.0e-6F ? scaled(v, 1.0F / length) : Vector3{};
}

[[nodiscard]] Vector3 from(const board::ImuSample& sample) noexcept
{
    return {sample.acceleration_x_mps2, sample.acceleration_y_mps2,
            sample.acceleration_z_mps2};
}

[[nodiscard]] Vector3 rates_from(const board::ImuSample& sample) noexcept
{
    return {sample.angular_rate_x_dps, sample.angular_rate_y_dps,
            sample.angular_rate_z_dps};
}

}  // namespace

GravityCalibration::GravityCalibration(const MountFacing facing) noexcept : facing_(facing) {}

CalibrationState GravityCalibration::update(const board::ImuSample& sample) noexcept
{
    if (!sample.valid) {
        still_count_ = 0;
        accumulator_ = {};
        gyro_accumulator_ = {};
        has_previous_ = false;
        has_previous_us_ = false;
        return state_;
    }

    const auto measured = from(sample);
    const auto magnitude_g = magnitude(measured) / kStandardGravityMps2;

    // Follow the car's attitude before the timestamp moves on, since tracking is defined
    // over the interval that just elapsed.
    if (state_ == CalibrationState::ready) {
        track_attitude(sample, magnitude_g);
    }
    previous_us_ = sample.monotonic_us;
    has_previous_us_ = sample.monotonic_us > 0;

    // At rest the only acceleration is gravity, so anything far from 1 g is movement.
    const auto near_rest = std::fabs(magnitude_g - 1.0F) <= kRestMagnitudeToleranceG;
    // With no previous sample there is no evidence of movement, so the first sample is
    // allowed to count. It still takes a full run of near-rest samples to qualify, so one
    // unlucky first reading cannot produce a reference on its own.
    const auto rates = rates_from(sample);
    auto steady = true;
    if (has_previous_) {
        const auto change = magnitude(subtract(measured, previous_)) / kStandardGravityMps2;
        const auto turn = magnitude(subtract(rates, previous_rates_));
        steady = change <= kRestJitterToleranceG && turn <= kRestRateJitterDps;
    }
    previous_ = measured;
    previous_rates_ = rates;
    has_previous_ = true;

    if (!near_rest || !steady) {
        // Movement resets the run. A reference already held is kept: re-learning while
        // the car is moving would drag the zero point around mid-session.
        still_count_ = 0;
        accumulator_ = {};
        gyro_accumulator_ = {};
        return state_;
    }

    accumulator_ = {accumulator_.x + measured.x, accumulator_.y + measured.y,
                    accumulator_.z + measured.z};
    gyro_accumulator_ = {gyro_accumulator_.x + rates.x, gyro_accumulator_.y + rates.y,
                         gyro_accumulator_.z + rates.z};
    ++still_count_;
    if (still_count_ >= kRestSamplesRequired) {
        adopt_reference();
    }
    return state_;
}

void GravityCalibration::track_attitude(const board::ImuSample& sample,
                                        const float magnitude_g) noexcept
{
    // The accelerometer measures gravity plus whatever the car is doing, and cannot say
    // which is which. Only when the total is near 1 g is it safe to read it as gravity.
    coasting_ = std::fabs(magnitude_g - 1.0F) > kAttitudeTrustToleranceG;

    if (!has_previous_us_ || sample.monotonic_us <= previous_us_) {
        return;
    }
    const auto elapsed_s =
        static_cast<float>(sample.monotonic_us - previous_us_) / 1.0e6F;
    if (elapsed_s > kMaximumIntegrationStepS) {
        return;
    }

    // Gravity is fixed in the world, so seen from a body that is turning it appears to
    // rotate the opposite way: the rate of change of a world-fixed vector expressed in
    // body axes is -omega x v. Removing the zero-rate offset first is what keeps this
    // from drifting, since integrating a constant bias is a ramp.
    const auto rate = scaled(subtract(rates_from(sample), gyro_bias_), kRadiansPerDegree);
    const auto turned = subtract(reference_, scaled(cross(rate, reference_), elapsed_s));
    const auto length = magnitude(turned);
    if (length > 1.0e-3F) {
        reference_ = scaled(turned, 1.0F / length);
    }

    // Then pull gently back toward the accelerometer, which has no drift, but only while
    // it is plausibly measuring gravity alone. Under cornering or braking it is not, and
    // believing it there would absorb the very acceleration being measured.
    if (coasting_) {
        return;
    }
    auto gain = elapsed_s / kAttitudeTimeConstantS;
    if (gain > 1.0F) {
        gain = 1.0F;
    }
    const auto observed = normalised(from(sample));
    const auto blended = Vector3{reference_.x * (1.0F - gain) + observed.x * gain,
                                 reference_.y * (1.0F - gain) + observed.y * gain,
                                 reference_.z * (1.0F - gain) + observed.z * gain};
    if (magnitude(blended) > 1.0e-3F) {
        reference_ = normalised(blended);
    }
}

GravityCalibration::Frame GravityCalibration::frame_for(const Vector3& reference) const noexcept
{
    Frame frame{};
    // An accelerometer at rest measures specific force, which points up, so the reference
    // is already "up" and must not be negated. Negating it inverted the lateral axis and
    // reported a right-hand corner as a left-hand one.
    frame.up = reference;
    // The screen normal is the device Z axis. Forward is along it, sign depending on
    // which way the unit faces; right then follows from forward and up.
    const Vector3 screen_normal{0.0F, 0.0F,
                                facing_ == MountFacing::screen_to_driver ? -1.0F : 1.0F};

    auto forward = subtract(screen_normal, scaled(frame.up, dot(screen_normal, frame.up)));
    if (magnitude(forward) <= 1.0e-3F) {
        // The unit is lying flat, so the screen normal is vertical and says nothing about
        // forward. Fall back to the device Y axis projected into the horizontal plane so
        // the meter still centres and still reports magnitude, even if the axis labels
        // are then arbitrary.
        const Vector3 fallback{0.0F, 1.0F, 0.0F};
        forward = subtract(fallback, scaled(frame.up, dot(fallback, frame.up)));
        if (magnitude(forward) <= 1.0e-3F) {
            return frame;
        }
    }

    frame.longitudinal = normalised(forward);
    frame.lateral = normalised(cross(frame.longitudinal, frame.up));
    frame.valid = true;
    return frame;
}

PlanarG GravityCalibration::resolve(const board::ImuSample& sample) const noexcept
{
    if (state_ != CalibrationState::ready || !sample.valid) {
        return {};
    }
    const auto frame = frame_for(reference_);
    if (!frame.valid) {
        return {};
    }
    // Apply the scale correction, then remove gravity. What remains is what the car did.
    const auto corrected = scaled(from(sample), scale_);
    const auto linear = subtract(corrected, scaled(reference_, kStandardGravityMps2));

    PlanarG resolved{};
    resolved.lateral_g = dot(linear, frame.lateral) / kStandardGravityMps2;
    resolved.longitudinal_g = dot(linear, frame.longitudinal) / kStandardGravityMps2;
    resolved.vertical_g = dot(linear, frame.up) / kStandardGravityMps2;
    resolved.valid = true;
    return resolved;
}

void GravityCalibration::adopt_reference() noexcept
{
    const auto average = scaled(accumulator_, 1.0F / static_cast<float>(still_count_));
    const auto length = magnitude(average);
    if (length <= 1.0e-3F) {
        still_count_ = 0;
        accumulator_ = {};
        gyro_accumulator_ = {};
        return;
    }

    const auto reference = scaled(average, 1.0F / length);
    if (!frame_for(reference).valid) {
        still_count_ = 0;
        accumulator_ = {};
        gyro_accumulator_ = {};
        return;
    }

    reference_ = reference;
    // The magnitude at rest is 1 g by definition, so any difference is sensor scale.
    scale_ = kStandardGravityMps2 / length;
    // Nothing is turning, so whatever the gyroscope reports here is its zero-rate offset.
    gyro_bias_ = scaled(gyro_accumulator_, 1.0F / static_cast<float>(still_count_));

    accumulator_ = {};
    gyro_accumulator_ = {};
    still_count_ = 0;
    coasting_ = false;
    state_ = CalibrationState::ready;
}

void GravityCalibration::restart() noexcept
{
    state_ = CalibrationState::collecting;
    reference_ = {};
    scale_ = 1.0F;
    gyro_bias_ = {};
    coasting_ = false;
    accumulator_ = {};
    gyro_accumulator_ = {};
    previous_ = {};
    previous_rates_ = {};
    still_count_ = 0;
    has_previous_ = false;
    previous_us_ = 0;
    has_previous_us_ = false;
}

void GravityCalibration::set_facing(const MountFacing facing) noexcept
{
    if (facing_ != facing) {
        facing_ = facing;
        restart();
    }
}

CalibrationState GravityCalibration::state() const noexcept { return state_; }
Vector3 GravityCalibration::gravity() const noexcept { return reference_; }
float GravityCalibration::scale_correction() const noexcept { return scale_; }
std::size_t GravityCalibration::still_samples() const noexcept { return still_count_; }
Vector3 GravityCalibration::gyro_bias() const noexcept { return gyro_bias_; }
bool GravityCalibration::coasting() const noexcept { return coasting_; }

const char* calibration_state_name(const CalibrationState state) noexcept
{
    switch (state) {
    case CalibrationState::collecting:
        return "collecting";
    case CalibrationState::ready:
        return "ready";
    }
    return "unknown";
}

}  // namespace track_timer::imu
