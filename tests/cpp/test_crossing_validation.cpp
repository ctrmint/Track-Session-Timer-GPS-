#include "track_timer/timing/crossing_validation.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

namespace {

using track_timer::domain::FixRejectReason;
using track_timer::domain::FixType;
using track_timer::domain::GnssFix;
using track_timer::timing::CrossingQualityThresholds;
using track_timer::timing::CrossingValidationDecision;
using track_timer::timing::CrossingValidationResult;
using track_timer::track::DirectedGateDefinition;
using track_timer::track::LocalPoint;

GnssFix fix(const std::uint32_t sequence, const float heading_deg = 90.0F,
            const float speed_mps = 10.0F)
{
    GnssFix value{};
    value.measurement_time_ns = 1'000'000'000 + sequence * 40'000'000;
    value.arrival_monotonic_us = 1'000'000 + sequence * 40'000;
    value.speed_mps = speed_mps;
    value.heading_deg = heading_deg;
    value.horizontal_accuracy_m = 0.8F;
    value.speed_accuracy_mps = 0.2F;
    value.heading_accuracy_deg = 2.0F;
    value.sequence_number = sequence;
    value.num_satellites = 12;
    value.fix_type = FixType::fix_3d;
    value.reject_reason = FixRejectReason::none;
    value.accepted_for_timing = true;
    return value;
}

DirectedGateDefinition eastbound_gate()
{
    DirectedGateDefinition gate{};
    gate.local_left = {0.0, 2.0};
    gate.local_right = {0.0, -2.0};
    gate.direction_heading_deg = 90.0;
    gate.heading_tolerance_deg = 20.0;
    gate.minimum_crossing_speed_mps = 2.0;
    gate.rearm_corridor_m = 10.0;
    return gate;
}

CrossingValidationDecision evaluate(
    const GnssFix& fix_0, const LocalPoint& position_0, const GnssFix& fix_1,
    const LocalPoint& position_1, const DirectedGateDefinition& gate,
    const CrossingQualityThresholds& thresholds = {})
{
    const auto intersection = track_timer::timing::intersect_movement_with_gate(
        position_0, position_1, gate.local_left, gate.local_right);
    return track_timer::timing::validate_crossing(
        fix_0, position_0, fix_1, position_1, gate, intersection, thresholds);
}

void expect(const CrossingValidationDecision& decision,
            const CrossingValidationResult result)
{
    assert(decision.result == result);
}

void assert_near(const double actual, const double expected,
                 const double tolerance = 1.0e-6)
{
    assert(std::abs(actual - expected) <= tolerance);
}

}  // namespace

int main()
{
    using namespace track_timer::timing;

    const auto gate = eastbound_gate();
    const LocalPoint west{-10.0, 0.0};
    const LocalPoint line{0.0, 0.0};
    const LocalPoint east{10.0, 0.0};

    const auto accepted = evaluate(fix(40), west, fix(41), east, gate);
    expect(accepted, CrossingValidationResult::accepted);
    assert(accepted.segment_sequence_0 == 40);
    assert(accepted.segment_sequence_1 == 41);
    assert_near(accepted.intersection_fraction, 0.5);
    assert_near(accepted.signed_start_distance_m, -10.0);
    assert_near(accepted.signed_end_distance_m, 10.0);
    assert_near(accepted.crossing_speed_mps, 10.0);
    assert_near(accepted.crossing_heading_deg, 90.0);
    assert_near(accepted.heading_difference_deg, 0.0);
    assert(accepted.thresholds.minimum_satellites == kDefaultMinimumSatellites);
    assert_near(accepted.thresholds.maximum_horizontal_accuracy_m,
                kDefaultMaximumHorizontalAccuracyM);

    const auto before_to_on = evaluate(fix(1), west, fix(2), line, gate);
    const auto on_to_after = evaluate(fix(2), line, fix(3), east, gate);
    assert((before_to_on.result == CrossingValidationResult::accepted ? 1 : 0) +
               (on_to_after.result == CrossingValidationResult::accepted ? 1 : 0) ==
           1);
    expect(before_to_on, CrossingValidationResult::accepted);
    expect(on_to_after, CrossingValidationResult::no_geometric_intersection);

    expect(evaluate(fix(1, 270.0F), east, fix(2, 270.0F), west, gate),
           CrossingValidationResult::wrong_direction);
    expect(evaluate(fix(1, 270.0F), west, fix(2, 270.0F), east, gate),
           CrossingValidationResult::heading_outside_tolerance);
    expect(evaluate(fix(1, 90.0F, 1.0F), west, fix(2, 90.0F, 1.0F), east, gate),
           CrossingValidationResult::below_minimum_speed);

    auto rejected = fix(2);
    rejected.accepted_for_timing = false;
    rejected.reject_reason = FixRejectReason::stale;
    expect(evaluate(fix(1), west, rejected, east, gate),
           CrossingValidationResult::source_fix_rejected);

    auto invalid = fix(2);
    invalid.heading_deg = std::numeric_limits<float>::quiet_NaN();
    expect(evaluate(fix(1), west, invalid, east, gate),
           CrossingValidationResult::invalid_measurement);

    auto two_dimensional = fix(2);
    two_dimensional.fix_type = FixType::fix_2d;
    expect(evaluate(fix(1), west, two_dimensional, east, gate),
           CrossingValidationResult::insufficient_fix_type);
    CrossingQualityThresholds allow_2d{};
    allow_2d.minimum_fix = CrossingFixRequirement::two_dimensional;
    auto first_2d = fix(1);
    first_2d.fix_type = FixType::fix_2d;
    expect(evaluate(first_2d, west, two_dimensional, east, gate, allow_2d),
           CrossingValidationResult::accepted);

    auto few_satellites = fix(2);
    few_satellites.num_satellites = kDefaultMinimumSatellites - 1;
    expect(evaluate(fix(1), west, few_satellites, east, gate),
           CrossingValidationResult::insufficient_satellites);

    auto poor_position = fix(2);
    poor_position.horizontal_accuracy_m = kDefaultMaximumHorizontalAccuracyM + 0.1F;
    expect(evaluate(fix(1), west, poor_position, east, gate),
           CrossingValidationResult::excessive_horizontal_accuracy);
    auto poor_speed = fix(2);
    poor_speed.speed_accuracy_mps = kDefaultMaximumSpeedAccuracyMps + 0.1F;
    expect(evaluate(fix(1), west, poor_speed, east, gate),
           CrossingValidationResult::excessive_speed_accuracy);
    auto poor_heading = fix(2);
    poor_heading.heading_accuracy_deg = kDefaultMaximumHeadingAccuracyDeg + 0.1F;
    expect(evaluate(fix(1), west, poor_heading, east, gate),
           CrossingValidationResult::excessive_heading_accuracy);

    auto invalid_thresholds = CrossingQualityThresholds{};
    invalid_thresholds.maximum_horizontal_accuracy_m = 0.0F;
    expect(evaluate(fix(1), west, fix(2), east, gate, invalid_thresholds),
           CrossingValidationResult::invalid_configuration);
    invalid_thresholds = CrossingQualityThresholds{};
    invalid_thresholds.minimum_fix = static_cast<CrossingFixRequirement>(99);
    expect(evaluate(fix(1), west, fix(2), east, gate, invalid_thresholds),
           CrossingValidationResult::invalid_configuration);

    auto invalid_gate = gate;
    invalid_gate.direction_heading_deg = 360.0;
    expect(evaluate(fix(1), west, fix(2), east, invalid_gate),
           CrossingValidationResult::invalid_configuration);

    expect(evaluate(fix(1), {-10.0, 3.0}, fix(2), {10.0, 3.0}, gate),
           CrossingValidationResult::no_geometric_intersection);
    expect(evaluate(fix(1), {0.0, 1.0}, fix(2), {0.0, -1.0}, gate),
           CrossingValidationResult::no_geometric_intersection);

    auto northbound_gate = gate;
    northbound_gate.local_left = {-2.0, 0.0};
    northbound_gate.local_right = {2.0, 0.0};
    northbound_gate.direction_heading_deg = 0.0;
    const auto wraparound = evaluate(fix(1, 359.0F), {0.0, -10.0},
                                     fix(2, 1.0F), {0.0, 10.0}, northbound_gate);
    expect(wraparound, CrossingValidationResult::accepted);
    assert_near(wraparound.crossing_heading_deg, 0.0);
    assert_near(circular_heading_difference_deg(359.0F, 1.0F), 2.0);
    assert_near(circular_heading_difference_deg(1.0F, 359.0F), 2.0);

    const std::array all_results{
        CrossingValidationResult::accepted,
        CrossingValidationResult::invalid_configuration,
        CrossingValidationResult::no_geometric_intersection,
        CrossingValidationResult::source_fix_rejected,
        CrossingValidationResult::invalid_measurement,
        CrossingValidationResult::insufficient_fix_type,
        CrossingValidationResult::insufficient_satellites,
        CrossingValidationResult::excessive_horizontal_accuracy,
        CrossingValidationResult::excessive_speed_accuracy,
        CrossingValidationResult::excessive_heading_accuracy,
        CrossingValidationResult::wrong_direction,
        CrossingValidationResult::below_minimum_speed,
        CrossingValidationResult::heading_outside_tolerance,
    };
    for (const auto result : all_results) {
        assert(crossing_validation_result_name(result)[0] != '\0');
    }

    std::cout << "Directed crossing speed, heading, and GNSS-quality validation passed\n";
    return 0;
}
