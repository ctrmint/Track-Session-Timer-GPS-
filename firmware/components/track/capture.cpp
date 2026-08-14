#include "track_timer/track/capture.hpp"

#include "track_timer/track/projection.hpp"

#include <cmath>

namespace track_timer::track {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRadiansToDegrees = 180.0 / kPi;

bool usable_fix_type(const domain::FixType type) noexcept
{
    return type == domain::FixType::fix_2d || type == domain::FixType::fix_3d ||
           type == domain::FixType::gnss_dead_reckoning;
}

bool valid_request(const GateCaptureRequest& request) noexcept
{
    return std::isfinite(request.expected_heading_deg) &&
           request.expected_heading_deg >= 0.0 && request.expected_heading_deg < 360.0 &&
           std::isfinite(request.width_m) && request.width_m >= kMinimumCapturedGateWidthM &&
           request.width_m <= kMaximumCapturedGateWidthM &&
           std::isfinite(request.heading_tolerance_deg) &&
           request.heading_tolerance_deg >= 0.0 &&
           request.heading_tolerance_deg <= 180.0 &&
           std::isfinite(request.minimum_crossing_speed_mps) &&
           request.minimum_crossing_speed_mps > 0.0 &&
           request.minimum_crossing_speed_mps <= 150.0 &&
           std::isfinite(request.rearm_corridor_m) && request.rearm_corridor_m > 0.0 &&
           request.rearm_corridor_m <= 1'000.0;
}

bool valid_fix_position(const domain::GnssFix& fix) noexcept
{
    return std::isfinite(fix.latitude_deg) && std::isfinite(fix.longitude_deg) &&
           fix.latitude_deg >= -85.0 && fix.latitude_deg <= 85.0 &&
           fix.longitude_deg >= -180.0 && fix.longitude_deg <= 180.0;
}

GeographicPoint from_local(const CircuitProjection& projection,
                           const LocalPoint& point) noexcept
{
    auto longitude = projection.reference.longitude_deg +
                     point.east_m / projection.east_metres_per_radian * kRadiansToDegrees;
    if (longitude > 180.0) {
        longitude -= 360.0;
    }
    else if (longitude < -180.0) {
        longitude += 360.0;
    }
    return {projection.reference.latitude_deg +
                point.north_m / projection.north_metres_per_radian * kRadiansToDegrees,
            longitude};
}

}  // namespace

GateCaptureResult capture_stationary_gate(const domain::GnssFix& fix,
                                          const GateCaptureRequest& request,
                                          const bool session_active,
                                          GateCapturePreview& output) noexcept
{
    if (session_active) {
        return GateCaptureResult::active_session;
    }
    if (!valid_request(request)) {
        return GateCaptureResult::invalid_request;
    }
    if (!fix.accepted_for_timing || fix.reject_reason != domain::FixRejectReason::none ||
        !usable_fix_type(fix.fix_type) || !valid_fix_position(fix) ||
        fix.measurement_time_ns == domain::kUnavailableTime || !std::isfinite(fix.speed_mps) ||
        fix.speed_mps < 0.0F || !std::isfinite(fix.horizontal_accuracy_m)) {
        return GateCaptureResult::unusable_fix;
    }
    if (fix.speed_mps > kMaximumCaptureSpeedMps) {
        return GateCaptureResult::moving;
    }
    if (fix.horizontal_accuracy_m <= 0.0F ||
        fix.horizontal_accuracy_m > kMaximumCaptureHorizontalAccuracyM) {
        return GateCaptureResult::poor_accuracy;
    }

    const GeographicPoint center{fix.latitude_deg, fix.longitude_deg};
    CircuitProjection projection{};
    if (configure_circuit_projection(center, projection) != ProjectionResult::projected) {
        return GateCaptureResult::projection_failed;
    }

    const auto heading_rad = request.expected_heading_deg * kPi / 180.0;
    const auto half_width_m = request.width_m / 2.0;
    const LocalPoint left{-std::cos(heading_rad) * half_width_m,
                          std::sin(heading_rad) * half_width_m};
    const LocalPoint right{-left.east_m, -left.north_m};

    GateCapturePreview candidate{};
    candidate.center = center;
    candidate.width_m = request.width_m;
    candidate.source_horizontal_accuracy_m = fix.horizontal_accuracy_m;
    candidate.source_fix_sequence = fix.sequence_number;
    candidate.gate.left = from_local(projection, left);
    candidate.gate.right = from_local(projection, right);
    candidate.gate.local_left = left;
    candidate.gate.local_right = right;
    candidate.gate.direction_heading_deg = request.expected_heading_deg;
    candidate.gate.heading_tolerance_deg = request.heading_tolerance_deg;
    candidate.gate.minimum_crossing_speed_mps = request.minimum_crossing_speed_mps;
    candidate.gate.rearm_corridor_m = request.rearm_corridor_m;
    output = candidate;
    return GateCaptureResult::captured;
}

EndpointCaptureResult capture_stationary_endpoint(
    const domain::GnssFix& fix, const std::int64_t evaluation_monotonic_us,
    const bool session_active, GeographicPoint& output) noexcept
{
    if (session_active) {
        return EndpointCaptureResult::active_session;
    }
    if (!fix.accepted_for_timing || fix.reject_reason != domain::FixRejectReason::none ||
        !usable_fix_type(fix.fix_type) || !valid_fix_position(fix) ||
        fix.measurement_time_ns == domain::kUnavailableTime ||
        fix.arrival_monotonic_us == domain::kUnavailableTime ||
        !std::isfinite(fix.speed_mps) || fix.speed_mps < 0.0F ||
        !std::isfinite(fix.horizontal_accuracy_m)) {
        return EndpointCaptureResult::unusable_fix;
    }
    if (evaluation_monotonic_us < fix.arrival_monotonic_us ||
        evaluation_monotonic_us - fix.arrival_monotonic_us > kMaximumCaptureFixAgeUs) {
        return EndpointCaptureResult::stale_fix;
    }
    if (fix.speed_mps > kMaximumCaptureSpeedMps) {
        return EndpointCaptureResult::moving;
    }
    if (fix.horizontal_accuracy_m <= 0.0F ||
        fix.horizontal_accuracy_m > kMaximumCaptureHorizontalAccuracyM) {
        return EndpointCaptureResult::poor_accuracy;
    }
    output = {fix.latitude_deg, fix.longitude_deg};
    return EndpointCaptureResult::captured;
}

const char* gate_capture_result_name(const GateCaptureResult result) noexcept
{
    switch (result) {
    case GateCaptureResult::captured:
        return "captured";
    case GateCaptureResult::active_session:
        return "active-session";
    case GateCaptureResult::moving:
        return "moving";
    case GateCaptureResult::unusable_fix:
        return "unusable-fix";
    case GateCaptureResult::poor_accuracy:
        return "poor-accuracy";
    case GateCaptureResult::invalid_request:
        return "invalid-request";
    case GateCaptureResult::projection_failed:
        return "projection-failed";
    }
    return "invalid-request";
}

const char* endpoint_capture_result_name(const EndpointCaptureResult result) noexcept
{
    switch (result) {
    case EndpointCaptureResult::captured:
        return "captured";
    case EndpointCaptureResult::active_session:
        return "active-session";
    case EndpointCaptureResult::moving:
        return "moving";
    case EndpointCaptureResult::stale_fix:
        return "stale-fix";
    case EndpointCaptureResult::unusable_fix:
        return "unusable-fix";
    case EndpointCaptureResult::poor_accuracy:
        return "poor-accuracy";
    }
    return "unusable-fix";
}

}  // namespace track_timer::track
