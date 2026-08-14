#include "track_timer/timing/intersection.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

namespace {

using track_timer::timing::SegmentIntersectionResult;
using track_timer::track::LocalPoint;

struct IntersectionVector {
    const char* name;
    LocalPoint movement_start;
    LocalPoint movement_end;
    LocalPoint gate_left;
    LocalPoint gate_right;
    SegmentIntersectionResult expected;
    double expected_movement_fraction;
    double expected_gate_fraction;
};

struct ReferenceIntersection {
    SegmentIntersectionResult result;
    long double movement_fraction;
    long double gate_fraction;
};

[[nodiscard]] long double cross(const LocalPoint& lhs, const LocalPoint& rhs)
{
    return static_cast<long double>(lhs.east_m) * rhs.north_m -
           static_cast<long double>(lhs.north_m) * rhs.east_m;
}

[[nodiscard]] LocalPoint subtract(const LocalPoint& lhs, const LocalPoint& rhs)
{
    return {lhs.east_m - rhs.east_m, lhs.north_m - rhs.north_m};
}

[[nodiscard]] bool finite_point(const LocalPoint& point)
{
    return std::isfinite(point.east_m) && std::isfinite(point.north_m);
}

[[nodiscard]] ReferenceIntersection reference_intersection(
    const IntersectionVector& vector)
{
    using namespace track_timer::timing;

    if (!finite_point(vector.movement_start) || !finite_point(vector.movement_end) ||
        !finite_point(vector.gate_left) || !finite_point(vector.gate_right)) {
        return {SegmentIntersectionResult::invalid_coordinate, 0.0L, 0.0L};
    }

    const auto movement = subtract(vector.movement_end, vector.movement_start);
    const auto gate = subtract(vector.gate_right, vector.gate_left);
    const auto offset = subtract(vector.gate_left, vector.movement_start);
    const auto movement_length = std::hypotl(movement.east_m, movement.north_m);
    const auto gate_length = std::hypotl(gate.east_m, gate.north_m);
    if (movement_length <= kMinimumIntersectionSegmentLengthM) {
        return {SegmentIntersectionResult::degenerate_movement, 0.0L, 0.0L};
    }
    if (gate_length <= kMinimumIntersectionSegmentLengthM) {
        return {SegmentIntersectionResult::degenerate_gate, 0.0L, 0.0L};
    }

    const auto denominator = cross(movement, gate);
    const auto parallel_tolerance =
        static_cast<long double>(kIntersectionParallelSineTolerance) *
        movement_length * gate_length;
    if (std::abs(denominator) <= parallel_tolerance) {
        const auto offset_length = std::hypotl(offset.east_m, offset.north_m);
        const auto collinear_tolerance =
            static_cast<long double>(kIntersectionParallelSineTolerance) *
            movement_length * std::max(offset_length, 1.0L);
        return {std::abs(cross(offset, movement)) <= collinear_tolerance
                    ? SegmentIntersectionResult::collinear
                    : SegmentIntersectionResult::parallel,
                0.0L, 0.0L};
    }

    const auto movement_fraction = cross(offset, gate) / denominator;
    const auto gate_fraction = cross(offset, movement) / denominator;
    const auto movement_tolerance =
        static_cast<long double>(kIntersectionEndpointToleranceM) / movement_length;
    const auto gate_tolerance =
        static_cast<long double>(kIntersectionEndpointToleranceM) / gate_length;
    if (movement_fraction < -movement_tolerance ||
        movement_fraction > 1.0L + movement_tolerance ||
        gate_fraction < -gate_tolerance || gate_fraction > 1.0L + gate_tolerance) {
        return {SegmentIntersectionResult::no_intersection, movement_fraction,
                gate_fraction};
    }
    if (movement_fraction <= movement_tolerance) {
        return {SegmentIntersectionResult::movement_start_not_owned,
                movement_fraction, gate_fraction};
    }
    return {SegmentIntersectionResult::intersection,
            std::clamp(movement_fraction, 0.0L, 1.0L),
            std::clamp(gate_fraction, 0.0L, 1.0L)};
}

void assert_near(const double actual, const double expected,
                 const double tolerance = 1.0e-12)
{
    assert(std::abs(actual - expected) <= tolerance);
}

}  // namespace

int main()
{
    using namespace track_timer::timing;

    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const std::array vectors{
        IntersectionVector{"proper crossing", {-5.0, 0.0}, {5.0, 0.0},
                           {0.0, -2.0}, {0.0, 2.0},
                           SegmentIntersectionResult::intersection, 0.5, 0.5},
        IntersectionVector{"movement end is owned", {-1.0, 0.0}, {0.0, 0.0},
                           {0.0, -1.0}, {0.0, 1.0},
                           SegmentIntersectionResult::intersection, 1.0, 0.5},
        IntersectionVector{"movement start is not owned", {0.0, 0.0}, {1.0, 0.0},
                           {0.0, -1.0}, {0.0, 1.0},
                           SegmentIntersectionResult::movement_start_not_owned, 0.0,
                           0.5},
        IntersectionVector{"gate left endpoint", {-1.0, 0.0}, {1.0, 0.0},
                           {0.0, 0.0}, {0.0, 2.0},
                           SegmentIntersectionResult::intersection, 0.5, 0.0},
        IntersectionVector{"gate right endpoint", {-1.0, 0.0}, {1.0, 0.0},
                           {0.0, -2.0}, {0.0, 0.0},
                           SegmentIntersectionResult::intersection, 0.5, 1.0},
        IntersectionVector{"movement does not reach gate", {-2.0, 0.0}, {-1.0, 0.0},
                           {0.0, -1.0}, {0.0, 1.0},
                           SegmentIntersectionResult::no_intersection, 0.0, 0.0},
        IntersectionVector{"intersection outside gate", {-1.0, 2.0}, {1.0, 2.0},
                           {0.0, -1.0}, {0.0, 1.0},
                           SegmentIntersectionResult::no_intersection, 0.0, 0.0},
        IntersectionVector{"parallel", {0.0, 0.0}, {10.0, 0.0},
                           {0.0, 1.0}, {10.0, 1.0},
                           SegmentIntersectionResult::parallel, 0.0, 0.0},
        IntersectionVector{"collinear", {0.0, 0.0}, {10.0, 0.0},
                           {2.0, 0.0}, {8.0, 0.0},
                           SegmentIntersectionResult::collinear, 0.0, 0.0},
        IntersectionVector{"near parallel rejected", {0.0, 0.0}, {1'000.0, 0.0},
                           {0.0, 1.0}, {1'000.0, 1.0 + 1.0e-10},
                           SegmentIntersectionResult::parallel, 0.0, 0.0},
        IntersectionVector{"near parallel crossing", {0.0, 0.0}, {1'000.0, 0.0},
                           {0.0, -1.0e-5}, {1'000.0, 1.0e-5},
                           SegmentIntersectionResult::intersection, 0.5, 0.5},
        IntersectionVector{"translated coordinates", {23'990.0, 23'990.0},
                           {24'000.0, 24'000.0}, {23'990.0, 24'000.0},
                           {24'000.0, 23'990.0},
                           SegmentIntersectionResult::intersection, 0.5, 0.5},
        IntersectionVector{"zero movement", {1.0, 1.0}, {1.0, 1.0},
                           {0.0, -1.0}, {0.0, 1.0},
                           SegmentIntersectionResult::degenerate_movement, 0.0, 0.0},
        IntersectionVector{"zero gate", {-1.0, 0.0}, {1.0, 0.0},
                           {0.0, 0.0}, {0.0, 0.0},
                           SegmentIntersectionResult::degenerate_gate, 0.0, 0.0},
        IntersectionVector{"invalid coordinate", {nan, 0.0}, {1.0, 0.0},
                           {0.0, -1.0}, {0.0, 1.0},
                           SegmentIntersectionResult::invalid_coordinate, 0.0, 0.0},
    };

    for (const auto& vector : vectors) {
        const auto production = intersect_movement_with_gate(
            vector.movement_start, vector.movement_end, vector.gate_left,
            vector.gate_right);
        const auto reference = reference_intersection(vector);
        assert(production.result == vector.expected);
        assert(reference.result == vector.expected);
        if (vector.expected == SegmentIntersectionResult::intersection) {
            assert_near(production.movement_fraction,
                        vector.expected_movement_fraction);
            assert_near(production.gate_fraction, vector.expected_gate_fraction);
            assert_near(production.movement_fraction,
                        static_cast<double>(reference.movement_fraction));
            assert_near(production.gate_fraction,
                        static_cast<double>(reference.gate_fraction));
        }
        (void)vector.name;
    }

    const LocalPoint before{-1.0, 0.0};
    const LocalPoint on{0.0, 0.0};
    const LocalPoint after{1.0, 0.0};
    const LocalPoint gate_left{0.0, -1.0};
    const LocalPoint gate_right{0.0, 1.0};
    const auto before_to_on =
        intersect_movement_with_gate(before, on, gate_left, gate_right);
    const auto on_to_after =
        intersect_movement_with_gate(on, after, gate_left, gate_right);
    assert((before_to_on.result == SegmentIntersectionResult::intersection ? 1 : 0) +
               (on_to_after.result == SegmentIntersectionResult::intersection ? 1 : 0) ==
           1);
    assert(before_to_on.movement_fraction == 1.0);
    assert(on_to_after.result == SegmentIntersectionResult::movement_start_not_owned);

    const auto repeated_on =
        intersect_movement_with_gate(on, on, gate_left, gate_right);
    assert(repeated_on.result == SegmentIntersectionResult::degenerate_movement);

    const auto position = intersect_movement_with_gate(
        {-5.0, 4.0}, {5.0, 6.0}, {0.0, 0.0}, {0.0, 10.0});
    assert(position.result == SegmentIntersectionResult::intersection);
    assert_near(position.position.east_m, 0.0);
    assert_near(position.position.north_m, 5.0);

    for (const auto result : {
             SegmentIntersectionResult::intersection,
             SegmentIntersectionResult::no_intersection,
             SegmentIntersectionResult::movement_start_not_owned,
             SegmentIntersectionResult::parallel,
             SegmentIntersectionResult::collinear,
             SegmentIntersectionResult::degenerate_movement,
             SegmentIntersectionResult::degenerate_gate,
             SegmentIntersectionResult::invalid_coordinate,
         }) {
        assert(segment_intersection_result_name(result)[0] != '\0');
    }

    std::cout << "Scale-aware half-open segment intersection vectors passed\n";
    return 0;
}
