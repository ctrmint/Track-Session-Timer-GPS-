#include "track_timer/track/projection.hpp"

#include <cmath>

namespace track_timer::track {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegreesToRadians = kPi / 180.0;
constexpr double kWgs84SemiMajorAxisM = 6'378'137.0;
constexpr double kWgs84Flattening = 1.0 / 298.257223563;
constexpr double kWgs84EccentricitySquared =
    kWgs84Flattening * (2.0 - kWgs84Flattening);
constexpr double kMaximumProjectionRadiusSquaredM =
    kMaximumCircuitProjectionRadiusM * kMaximumCircuitProjectionRadiusM;

bool valid_point(const GeographicPoint& point) noexcept
{
    return std::isfinite(point.latitude_deg) && std::isfinite(point.longitude_deg) &&
           point.latitude_deg >= -90.0 && point.latitude_deg <= 90.0 &&
           point.longitude_deg >= -180.0 && point.longitude_deg <= 180.0;
}

double normalized_longitude_delta_rad(const double point_longitude_deg,
                                      const double reference_longitude_deg) noexcept
{
    auto delta_deg = point_longitude_deg - reference_longitude_deg;
    if (delta_deg > 180.0) {
        delta_deg -= 360.0;
    }
    else if (delta_deg < -180.0) {
        delta_deg += 360.0;
    }
    return delta_deg * kDegreesToRadians;
}

}  // namespace

ProjectionResult configure_circuit_projection(const GeographicPoint& reference,
                                               CircuitProjection& output) noexcept
{
    if (!valid_point(reference) ||
        std::abs(reference.latitude_deg) >
            kMaximumCircuitProjectionReferenceLatitudeDeg) {
        return ProjectionResult::invalid_reference;
    }

    const auto latitude_rad = reference.latitude_deg * kDegreesToRadians;
    const auto sine_latitude = std::sin(latitude_rad);
    const auto denominator =
        std::sqrt(1.0 - kWgs84EccentricitySquared * sine_latitude * sine_latitude);
    const auto prime_vertical_radius_m = kWgs84SemiMajorAxisM / denominator;
    const auto meridional_radius_m =
        kWgs84SemiMajorAxisM * (1.0 - kWgs84EccentricitySquared) /
        (denominator * denominator * denominator);

    CircuitProjection candidate{};
    candidate.reference = reference;
    candidate.east_metres_per_radian = prime_vertical_radius_m * std::cos(latitude_rad);
    candidate.north_metres_per_radian = meridional_radius_m;
    candidate.configured = true;
    output = candidate;
    return ProjectionResult::projected;
}

ProjectionResult project_to_circuit_local(const CircuitProjection& projection,
                                          const GeographicPoint& point,
                                          LocalPoint& output) noexcept
{
    if (!projection.configured) {
        return ProjectionResult::unconfigured;
    }
    if (!valid_point(point)) {
        return ProjectionResult::invalid_point;
    }

    const auto east_m = normalized_longitude_delta_rad(
                            point.longitude_deg, projection.reference.longitude_deg) *
                        projection.east_metres_per_radian;
    const auto north_m = (point.latitude_deg - projection.reference.latitude_deg) *
                         kDegreesToRadians * projection.north_metres_per_radian;
    if (!std::isfinite(east_m) || !std::isfinite(north_m) ||
        east_m * east_m + north_m * north_m >
            kMaximumProjectionRadiusSquaredM) {
        return ProjectionResult::outside_supported_radius;
    }

    output = {east_m, north_m};
    return ProjectionResult::projected;
}

const char* projection_result_name(const ProjectionResult result) noexcept
{
    switch (result) {
    case ProjectionResult::projected:
        return "projected";
    case ProjectionResult::unconfigured:
        return "unconfigured";
    case ProjectionResult::invalid_reference:
        return "invalid-reference";
    case ProjectionResult::invalid_point:
        return "invalid-point";
    case ProjectionResult::outside_supported_radius:
        return "outside-supported-radius";
    }
    return "invalid-point";
}

}  // namespace track_timer::track
