#include "track_timer/ui/imu_meter.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace track_timer::ui {
namespace {

float clamp_g(const float value) noexcept
{
    return std::max(-kImuDisplayLimitG, std::min(kImuDisplayLimitG, value));
}

PlanarAcceleration rotate_sample(const ImuMeterInput& input) noexcept
{
    const auto x = input.sample.acceleration_x_mps2 / kStandardGravityMps2;
    const auto y = input.sample.acceleration_y_mps2 / kStandardGravityMps2;
    PlanarAcceleration point{};

    switch (input.orientation) {
    case board::DisplayOrientation::degrees_0:
        point = {x, y, input.x_axis_valid, input.y_axis_valid};
        break;
    case board::DisplayOrientation::degrees_90:
        point = {-y, x, input.y_axis_valid, input.x_axis_valid};
        break;
    case board::DisplayOrientation::degrees_180:
        point = {-x, -y, input.x_axis_valid, input.y_axis_valid};
        break;
    case board::DisplayOrientation::degrees_270:
        point = {y, -x, input.y_axis_valid, input.x_axis_valid};
        break;
    }

    point.lateral_g = point.lateral_valid ? clamp_g(point.lateral_g) : 0.0F;
    point.longitudinal_g = point.longitudinal_valid ? clamp_g(point.longitudinal_g) : 0.0F;
    return point;
}

void increment_saturated(std::uint32_t& value) noexcept
{
    if (value < std::numeric_limits<std::uint32_t>::max()) {
        ++value;
    }
}

}  // namespace

const ImuMeterSnapshot& ImuMeterController::update(const ImuMeterInput& input,
                                                   const bool session_active) noexcept
{
    if (session_active && !session_active_) {
        clear_measurements();
    }
    session_active_ = session_active;
    snapshot_.orientation = input.orientation;
    snapshot_.reset_allowed = !session_active;

    if (input.calibrating) {
        snapshot_.state = ImuMeterState::calibrating;
        snapshot_.current = {};
        return snapshot_;
    }

    const bool any_axis = input.x_axis_valid || input.y_axis_valid;
    if (!input.sample_available || !input.sample.valid || !any_axis) {
        snapshot_.state = ImuMeterState::unavailable;
        snapshot_.current = {};
        loss_seen_ = true;
        recovered_since_ms_ = 0;
        increment_saturated(snapshot_.rejected_samples);
        return snapshot_;
    }

    const auto point = rotate_sample(input);
    snapshot_.current = point;
    append(point);
    // Vertical does not rotate with the display: up is up however the unit is mounted.
    snapshot_.vertical_valid = input.sample_available && input.z_axis_valid;
    snapshot_.vertical_g =
        snapshot_.vertical_valid
            ? clamp_g(input.sample.acceleration_z_mps2 / kStandardGravityMps2)
            : 0.0F;

    update_peaks(point);
    increment_saturated(snapshot_.accepted_samples);

    if (!point.lateral_valid || !point.longitudinal_valid) {
        snapshot_.state = ImuMeterState::partial;
        loss_seen_ = true;
        recovered_since_ms_ = 0;
        return snapshot_;
    }

    if (loss_seen_) {
        if (recovered_since_ms_ == 0) {
            recovered_since_ms_ = input.now_ms;
        }
        if (input.now_ms - recovered_since_ms_ < kImuRecoveredNoticeMs) {
            snapshot_.state = ImuMeterState::recovered;
            return snapshot_;
        }
        loss_seen_ = false;
        recovered_since_ms_ = 0;
    }

    snapshot_.state = ImuMeterState::ready;
    return snapshot_;
}

bool ImuMeterController::reset(const bool session_active) noexcept
{
    snapshot_.reset_allowed = !session_active;
    if (session_active) {
        return false;
    }
    clear_measurements();
    return true;
}

const ImuMeterSnapshot& ImuMeterController::snapshot() const noexcept
{
    return snapshot_;
}

PlanarAcceleration ImuMeterController::trail_point(
    const std::size_t chronological_index) const noexcept
{
    if (chronological_index >= snapshot_.trail_count) {
        return {};
    }
    const auto first = snapshot_.trail_count == kImuTrailCapacity ? snapshot_.trail_next : 0;
    return snapshot_.trail[(first + chronological_index) % kImuTrailCapacity];
}

void ImuMeterController::clear_measurements() noexcept
{
    snapshot_.current = {};
    snapshot_.vertical_g = 0.0F;
    snapshot_.vertical_valid = false;
    snapshot_.peaks = {};
    snapshot_.trail.fill({});
    snapshot_.trail_count = 0;
    snapshot_.trail_next = 0;
}

void ImuMeterController::append(const PlanarAcceleration point) noexcept
{
    snapshot_.trail[snapshot_.trail_next] = point;
    snapshot_.trail_next = (snapshot_.trail_next + 1) % kImuTrailCapacity;
    if (snapshot_.trail_count < kImuTrailCapacity) {
        ++snapshot_.trail_count;
    }
}

void ImuMeterController::update_peaks(const PlanarAcceleration& point) noexcept
{
    if (snapshot_.vertical_valid) {
        snapshot_.peaks.up_g = std::max(snapshot_.peaks.up_g, snapshot_.vertical_g);
        snapshot_.peaks.down_g = std::max(snapshot_.peaks.down_g, -snapshot_.vertical_g);
    }
    if (point.longitudinal_valid) {
        snapshot_.peaks.acceleration_g =
            std::max(snapshot_.peaks.acceleration_g, point.longitudinal_g);
        snapshot_.peaks.braking_g =
            std::max(snapshot_.peaks.braking_g, -point.longitudinal_g);
    }
    if (point.lateral_valid) {
        snapshot_.peaks.left_g = std::max(snapshot_.peaks.left_g, -point.lateral_g);
        snapshot_.peaks.right_g = std::max(snapshot_.peaks.right_g, point.lateral_g);
    }
    if (point.lateral_valid && point.longitudinal_valid) {
        const auto total = std::sqrt(point.lateral_g * point.lateral_g +
                                     point.longitudinal_g * point.longitudinal_g);
        if (total > snapshot_.peaks.total_g) {
            snapshot_.peaks.total_g = total;
            snapshot_.peaks.total_position = point;
        }
    }
}

const char* imu_meter_state_name(const ImuMeterState state) noexcept
{
    switch (state) {
    case ImuMeterState::calibrating:
        return "calibrating";
    case ImuMeterState::ready:
        return "ready";
    case ImuMeterState::partial:
        return "partial";
    case ImuMeterState::unavailable:
        return "unavailable";
    case ImuMeterState::recovered:
        return "recovered";
    }
    return "unavailable";
}

const char* imu_meter_status_text(const ImuMeterState state) noexcept
{
    switch (state) {
    case ImuMeterState::calibrating:
        return "CALIBRATING - KEEP DEVICE LEVEL";
    case ImuMeterState::ready:
        return "IMU READY";
    case ImuMeterState::partial:
        return "PARTIAL IMU DATA";
    case ImuMeterState::unavailable:
        return "IMU UNAVAILABLE - TIMER UNAFFECTED";
    case ImuMeterState::recovered:
        return "IMU RECOVERED";
    }
    return "IMU UNAVAILABLE - TIMER UNAFFECTED";
}

}  // namespace track_timer::ui
