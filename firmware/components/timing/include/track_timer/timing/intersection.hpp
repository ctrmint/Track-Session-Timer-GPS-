#pragma once

#include "track_timer/track/definition.hpp"

#include <cstdint>
#include <type_traits>

namespace track_timer::timing {

inline constexpr double kMinimumIntersectionSegmentLengthM = 0.000'001;
inline constexpr double kIntersectionEndpointToleranceM = 0.000'001;
inline constexpr double kIntersectionParallelSineTolerance = 1.0e-12;

enum class SegmentIntersectionResult : std::uint8_t {
    intersection,
    no_intersection,
    movement_start_not_owned,
    parallel,
    collinear,
    degenerate_movement,
    degenerate_gate,
    invalid_coordinate,
};

struct SegmentIntersection {
    SegmentIntersectionResult result{SegmentIntersectionResult::no_intersection};
    double movement_fraction{0.0};
    double gate_fraction{0.0};
    track::LocalPoint position{};
};

// Consecutive movement segments use (start, end] ownership. Gate segments use
// [left, right] ownership. This prevents a fix exactly on a gate from being
// attributed to both adjacent movement segments.
[[nodiscard]] SegmentIntersection intersect_movement_with_gate(
    const track::LocalPoint& movement_start,
    const track::LocalPoint& movement_end,
    const track::LocalPoint& gate_left,
    const track::LocalPoint& gate_right) noexcept;

[[nodiscard]] const char* segment_intersection_result_name(
    SegmentIntersectionResult result) noexcept;

static_assert(std::is_trivially_copyable_v<SegmentIntersection>);
static_assert(sizeof(SegmentIntersection) <= 40);

}  // namespace track_timer::timing
