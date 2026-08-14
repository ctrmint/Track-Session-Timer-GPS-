#pragma once

#include "track_timer/track/definition.hpp"

#include <cstdint>
#include <type_traits>

namespace track_timer::track {

inline constexpr double kMaximumCircuitProjectionRadiusM = 25'000.0;
inline constexpr double kMaximumCircuitProjectionReferenceLatitudeDeg = 85.0;

enum class ProjectionResult : std::uint8_t {
    projected,
    unconfigured,
    invalid_reference,
    invalid_point,
    outside_supported_radius,
};

struct CircuitProjection {
    GeographicPoint reference{};
    double east_metres_per_radian{0.0};
    double north_metres_per_radian{0.0};
    bool configured{false};
};

[[nodiscard]] ProjectionResult configure_circuit_projection(
    const GeographicPoint& reference, CircuitProjection& output) noexcept;
[[nodiscard]] ProjectionResult project_to_circuit_local(
    const CircuitProjection& projection, const GeographicPoint& point,
    LocalPoint& output) noexcept;
[[nodiscard]] const char* projection_result_name(ProjectionResult result) noexcept;

static_assert(std::is_trivially_copyable_v<CircuitProjection>);
static_assert(sizeof(CircuitProjection) <= 48);

}  // namespace track_timer::track
