#include "track_timer/imu/calibration.hpp"

#include <cmath>

namespace track_timer::imu {
namespace {

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

}  // namespace

GravityCalibration::GravityCalibration(const MountFacing facing) noexcept : facing_(facing) {}

CalibrationState GravityCalibration::update(const board::ImuSample& sample) noexcept
{
    if (!sample.valid) {
        still_count_ = 0;
        has_previous_ = false;
        return state_;
    }

    const auto measured = from(sample);
    const auto magnitude_g = magnitude(measured) / kStandardGravityMps2;

    // At rest the only acceleration is gravity, so anything far from 1 g is movement.
    const auto near_rest = std::fabs(magnitude_g - 1.0F) <= kRestMagnitudeToleranceG;
    // With no previous sample there is no evidence of movement, so the first sample is
    // allowed to count. It still takes a full run of near-rest samples to qualify, so one
    // unlucky first reading cannot produce a reference on its own.
    auto steady = true;
    if (has_previous_) {
        const auto change = magnitude(subtract(measured, previous_)) / kStandardGravityMps2;
        steady = change <= kRestJitterToleranceG;
    }
    previous_ = measured;
    has_previous_ = true;

    if (!near_rest || !steady) {
        // Movement resets the run. A reference already held is kept: re-learning while
        // the car is moving would drag the zero point around mid-session.
        still_count_ = 0;
        accumulator_ = {};
        return state_;
    }

    accumulator_ = {accumulator_.x + measured.x, accumulator_.y + measured.y,
                    accumulator_.z + measured.z};
    ++still_count_;
    if (still_count_ >= kRestSamplesRequired) {
        adopt_reference();
    }
    return state_;
}

void GravityCalibration::adopt_reference() noexcept
{
    const auto average = scaled(accumulator_, 1.0F / static_cast<float>(still_count_));
    const auto length = magnitude(average);
    if (length <= 1.0e-3F) {
        still_count_ = 0;
        accumulator_ = {};
        return;
    }

    reference_ = scaled(average, 1.0F / length);
    // The magnitude at rest is 1 g by definition, so any difference is sensor scale.
    scale_ = kStandardGravityMps2 / length;

    const Vector3 up{-reference_.x, -reference_.y, -reference_.z};
    // The screen normal is the device Z axis. Forward is along it, sign depending on
    // which way the unit faces; right then follows from forward and up.
    const Vector3 screen_normal{0.0F, 0.0F,
                                facing_ == MountFacing::screen_to_driver ? -1.0F : 1.0F};

    auto forward = subtract(screen_normal, scaled(up, dot(screen_normal, up)));
    if (magnitude(forward) <= 1.0e-3F) {
        // The unit is lying flat, so the screen normal is vertical and says nothing about
        // forward. Fall back to the device Y axis projected into the horizontal plane so
        // the meter still centres and still reports magnitude, even if the axis labels
        // are then arbitrary.
        const Vector3 fallback{0.0F, 1.0F, 0.0F};
        forward = subtract(fallback, scaled(up, dot(fallback, up)));
        if (magnitude(forward) <= 1.0e-3F) {
            still_count_ = 0;
            accumulator_ = {};
            return;
        }
    }

    longitudinal_axis_ = normalised(forward);
    lateral_axis_ = normalised(cross(longitudinal_axis_, up));
    accumulator_ = {};
    still_count_ = 0;
    state_ = CalibrationState::ready;
}

PlanarG GravityCalibration::resolve(const board::ImuSample& sample) const noexcept
{
    if (state_ != CalibrationState::ready || !sample.valid) {
        return {};
    }
    // Apply the scale correction, then remove gravity. What remains is what the car did.
    const auto corrected = scaled(from(sample), scale_);
    const auto linear = subtract(corrected, scaled(reference_, kStandardGravityMps2));
    return {dot(linear, lateral_axis_) / kStandardGravityMps2,
            dot(linear, longitudinal_axis_) / kStandardGravityMps2, true};
}

void GravityCalibration::restart() noexcept
{
    state_ = CalibrationState::collecting;
    reference_ = {};
    lateral_axis_ = {};
    longitudinal_axis_ = {};
    scale_ = 1.0F;
    accumulator_ = {};
    previous_ = {};
    still_count_ = 0;
    has_previous_ = false;
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
