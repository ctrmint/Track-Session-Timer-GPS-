#include "track_timer/timing/intersection.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace track_timer::timing {
namespace {

using track::LocalPoint;

constexpr double kParameterRoundoffTolerance =
    64.0 * std::numeric_limits<double>::epsilon();

[[nodiscard]] bool finite_point(const LocalPoint& point) noexcept
{
    return std::isfinite(point.east_m) && std::isfinite(point.north_m);
}

[[nodiscard]] LocalPoint subtract(const LocalPoint& lhs, const LocalPoint& rhs) noexcept
{
    return {lhs.east_m - rhs.east_m, lhs.north_m - rhs.north_m};
}

[[nodiscard]] double cross(const LocalPoint& lhs, const LocalPoint& rhs) noexcept
{
    return lhs.east_m * rhs.north_m - lhs.north_m * rhs.east_m;
}

[[nodiscard]] double parameter_tolerance(const double segment_length_m) noexcept
{
    return std::max(kParameterRoundoffTolerance,
                    kIntersectionEndpointToleranceM / segment_length_m);
}

[[nodiscard]] double clamp_unit(const double value) noexcept
{
    return std::clamp(value, 0.0, 1.0);
}

}  // namespace

SegmentIntersection intersect_movement_with_gate(
    const LocalPoint& movement_start, const LocalPoint& movement_end,
    const LocalPoint& gate_left, const LocalPoint& gate_right) noexcept
{
    if (!finite_point(movement_start) || !finite_point(movement_end) ||
        !finite_point(gate_left) || !finite_point(gate_right)) {
        return {SegmentIntersectionResult::invalid_coordinate};
    }

    const auto movement = subtract(movement_end, movement_start);
    const auto gate = subtract(gate_right, gate_left);
    const auto gate_offset = subtract(gate_left, movement_start);
    if (!finite_point(movement) || !finite_point(gate) || !finite_point(gate_offset)) {
        return {SegmentIntersectionResult::invalid_coordinate};
    }

    const auto movement_length_m = std::hypot(movement.east_m, movement.north_m);
    const auto gate_length_m = std::hypot(gate.east_m, gate.north_m);
    if (!std::isfinite(movement_length_m) || !std::isfinite(gate_length_m)) {
        return {SegmentIntersectionResult::invalid_coordinate};
    }
    if (movement_length_m <= kMinimumIntersectionSegmentLengthM) {
        return {SegmentIntersectionResult::degenerate_movement};
    }
    if (gate_length_m <= kMinimumIntersectionSegmentLengthM) {
        return {SegmentIntersectionResult::degenerate_gate};
    }

    const auto denominator = cross(movement, gate);
    const auto denominator_scale = movement_length_m * gate_length_m;
    if (!std::isfinite(denominator) || !std::isfinite(denominator_scale)) {
        return {SegmentIntersectionResult::invalid_coordinate};
    }

    const auto parallel_tolerance =
        kIntersectionParallelSineTolerance * denominator_scale;
    if (std::abs(denominator) <= parallel_tolerance) {
        const auto offset_length_m =
            std::hypot(gate_offset.east_m, gate_offset.north_m);
        const auto collinear_scale =
            movement_length_m * std::max(offset_length_m, 1.0);
        const auto offset_cross = cross(gate_offset, movement);
        if (!std::isfinite(offset_length_m) || !std::isfinite(collinear_scale) ||
            !std::isfinite(offset_cross)) {
            return {SegmentIntersectionResult::invalid_coordinate};
        }
        if (std::abs(offset_cross) <=
            kIntersectionParallelSineTolerance * collinear_scale) {
            return {SegmentIntersectionResult::collinear};
        }
        return {SegmentIntersectionResult::parallel};
    }

    const auto movement_fraction = cross(gate_offset, gate) / denominator;
    const auto gate_fraction = cross(gate_offset, movement) / denominator;
    if (!std::isfinite(movement_fraction) || !std::isfinite(gate_fraction)) {
        return {SegmentIntersectionResult::invalid_coordinate};
    }

    const auto movement_tolerance = parameter_tolerance(movement_length_m);
    const auto gate_tolerance = parameter_tolerance(gate_length_m);
    if (movement_fraction < -movement_tolerance ||
        movement_fraction > 1.0 + movement_tolerance ||
        gate_fraction < -gate_tolerance || gate_fraction > 1.0 + gate_tolerance) {
        return {SegmentIntersectionResult::no_intersection};
    }
    if (movement_fraction <= movement_tolerance) {
        return {SegmentIntersectionResult::movement_start_not_owned};
    }

    const auto owned_movement_fraction = clamp_unit(movement_fraction);
    const auto owned_gate_fraction = clamp_unit(gate_fraction);
    return {
        SegmentIntersectionResult::intersection,
        owned_movement_fraction,
        owned_gate_fraction,
        {movement_start.east_m + owned_movement_fraction * movement.east_m,
         movement_start.north_m + owned_movement_fraction * movement.north_m},
    };
}

const char* segment_intersection_result_name(
    const SegmentIntersectionResult result) noexcept
{
    switch (result) {
    case SegmentIntersectionResult::intersection:
        return "intersection";
    case SegmentIntersectionResult::no_intersection:
        return "no-intersection";
    case SegmentIntersectionResult::movement_start_not_owned:
        return "movement-start-not-owned";
    case SegmentIntersectionResult::parallel:
        return "parallel";
    case SegmentIntersectionResult::collinear:
        return "collinear";
    case SegmentIntersectionResult::degenerate_movement:
        return "degenerate-movement";
    case SegmentIntersectionResult::degenerate_gate:
        return "degenerate-gate";
    case SegmentIntersectionResult::invalid_coordinate:
        return "invalid-coordinate";
    }
    return "invalid-coordinate";
}

}  // namespace track_timer::timing
