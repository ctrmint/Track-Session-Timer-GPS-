#include "track_timer/timing/crossing_validation.hpp"

#include <algorithm>
#include <cmath>

namespace track_timer::timing {
namespace {

using domain::FixType;
using track::LocalPoint;

constexpr double kLineSideToleranceM = 0.000'001;
constexpr double kMinimumForwardAlignment = 1.0e-6;

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

[[nodiscard]] bool valid_thresholds(const CrossingQualityThresholds& thresholds) noexcept
{
    const auto valid_fix_requirement =
        thresholds.minimum_fix == CrossingFixRequirement::two_dimensional ||
        thresholds.minimum_fix == CrossingFixRequirement::three_dimensional;
    return valid_fix_requirement &&
           std::isfinite(thresholds.maximum_horizontal_accuracy_m) &&
           thresholds.maximum_horizontal_accuracy_m > 0.0F &&
           std::isfinite(thresholds.maximum_speed_accuracy_mps) &&
           thresholds.maximum_speed_accuracy_mps > 0.0F &&
           std::isfinite(thresholds.maximum_heading_accuracy_deg) &&
           thresholds.maximum_heading_accuracy_deg > 0.0F &&
           thresholds.maximum_heading_accuracy_deg <= 180.0F &&
           thresholds.minimum_satellites > 0;
}

[[nodiscard]] bool valid_gate_parameters(
    const track::DirectedGateDefinition& gate) noexcept
{
    return finite_point(gate.local_left) && finite_point(gate.local_right) &&
           std::isfinite(gate.direction_heading_deg) &&
           gate.direction_heading_deg >= 0.0 && gate.direction_heading_deg < 360.0 &&
           std::isfinite(gate.heading_tolerance_deg) &&
           gate.heading_tolerance_deg >= 0.0 && gate.heading_tolerance_deg <= 180.0 &&
           std::isfinite(gate.minimum_crossing_speed_mps) &&
           gate.minimum_crossing_speed_mps > 0.0;
}

[[nodiscard]] bool valid_measurement(const domain::GnssFix& fix) noexcept
{
    return std::isfinite(fix.speed_mps) && fix.speed_mps >= 0.0F &&
           std::isfinite(fix.heading_deg) && fix.heading_deg >= 0.0F &&
           fix.heading_deg < 360.0F && std::isfinite(fix.horizontal_accuracy_m) &&
           fix.horizontal_accuracy_m >= 0.0F &&
           std::isfinite(fix.speed_accuracy_mps) && fix.speed_accuracy_mps >= 0.0F &&
           std::isfinite(fix.heading_accuracy_deg) &&
           fix.heading_accuracy_deg >= 0.0F;
}

[[nodiscard]] bool meets_fix_requirement(
    const FixType fix_type, const CrossingFixRequirement requirement) noexcept
{
    if (fix_type == FixType::fix_3d) {
        return true;
    }
    return requirement == CrossingFixRequirement::two_dimensional &&
           fix_type == FixType::fix_2d;
}

[[nodiscard]] float normalize_heading_deg(float heading_deg) noexcept
{
    heading_deg = std::fmod(heading_deg, 360.0F);
    if (heading_deg < 0.0F) {
        heading_deg += 360.0F;
    }
    if (heading_deg >= 360.0F) {
        heading_deg = 0.0F;
    }
    return heading_deg;
}

[[nodiscard]] float interpolate_heading_deg(const float start_deg,
                                            const float end_deg,
                                            const double fraction) noexcept
{
    const auto shortest_delta_deg = std::remainder(end_deg - start_deg, 360.0F);
    return normalize_heading_deg(
        start_deg + static_cast<float>(fraction) * shortest_delta_deg);
}

[[nodiscard]] CrossingValidationDecision decision_for(
    const CrossingValidationResult result, const domain::GnssFix& fix_0,
    const domain::GnssFix& fix_1, const SegmentIntersection& intersection) noexcept
{
    CrossingValidationDecision decision{};
    decision.result = result;
    decision.segment_sequence_0 = fix_0.sequence_number;
    decision.segment_sequence_1 = fix_1.sequence_number;
    decision.intersection_fraction = intersection.movement_fraction;
    return decision;
}

}  // namespace

CrossingValidationDecision validate_crossing(
    const domain::GnssFix& fix_0, const LocalPoint& position_0,
    const domain::GnssFix& fix_1, const LocalPoint& position_1,
    const track::DirectedGateDefinition& gate,
    const SegmentIntersection& intersection,
    const CrossingQualityThresholds& thresholds) noexcept
{
    auto decision = decision_for(CrossingValidationResult::invalid_configuration,
                                 fix_0, fix_1, intersection);
    decision.thresholds = thresholds;
    if (!valid_thresholds(thresholds) || !valid_gate_parameters(gate)) {
        return decision;
    }
    if (intersection.result != SegmentIntersectionResult::intersection ||
        !std::isfinite(intersection.movement_fraction) ||
        intersection.movement_fraction <= 0.0 ||
        intersection.movement_fraction > 1.0) {
        decision.result = CrossingValidationResult::no_geometric_intersection;
        return decision;
    }
    if (!fix_0.accepted_for_timing || !fix_1.accepted_for_timing ||
        fix_0.reject_reason != domain::FixRejectReason::none ||
        fix_1.reject_reason != domain::FixRejectReason::none) {
        decision.result = CrossingValidationResult::source_fix_rejected;
        return decision;
    }
    if (!finite_point(position_0) || !finite_point(position_1) ||
        !valid_measurement(fix_0) || !valid_measurement(fix_1)) {
        decision.result = CrossingValidationResult::invalid_measurement;
        return decision;
    }
    if (!meets_fix_requirement(fix_0.fix_type, thresholds.minimum_fix) ||
        !meets_fix_requirement(fix_1.fix_type, thresholds.minimum_fix)) {
        decision.result = CrossingValidationResult::insufficient_fix_type;
        return decision;
    }
    if (fix_0.num_satellites < thresholds.minimum_satellites ||
        fix_1.num_satellites < thresholds.minimum_satellites) {
        decision.result = CrossingValidationResult::insufficient_satellites;
        return decision;
    }
    if (std::max(fix_0.horizontal_accuracy_m, fix_1.horizontal_accuracy_m) >
        thresholds.maximum_horizontal_accuracy_m) {
        decision.result = CrossingValidationResult::excessive_horizontal_accuracy;
        return decision;
    }
    if (std::max(fix_0.speed_accuracy_mps, fix_1.speed_accuracy_mps) >
        thresholds.maximum_speed_accuracy_mps) {
        decision.result = CrossingValidationResult::excessive_speed_accuracy;
        return decision;
    }
    if (std::max(fix_0.heading_accuracy_deg, fix_1.heading_accuracy_deg) >
        thresholds.maximum_heading_accuracy_deg) {
        decision.result = CrossingValidationResult::excessive_heading_accuracy;
        return decision;
    }

    const auto gate_vector = subtract(gate.local_right, gate.local_left);
    const auto gate_length_m = std::hypot(gate_vector.east_m, gate_vector.north_m);
    if (!std::isfinite(gate_length_m) ||
        gate_length_m <= kMinimumIntersectionSegmentLengthM) {
        return decision;
    }
    decision.signed_start_distance_m =
        cross(gate_vector, subtract(position_0, gate.local_left)) / gate_length_m;
    decision.signed_end_distance_m =
        cross(gate_vector, subtract(position_1, gate.local_left)) / gate_length_m;

    constexpr double kPi = 3.14159265358979323846;
    const auto heading_rad = gate.direction_heading_deg * kPi / 180.0;
    const LocalPoint expected_direction{std::sin(heading_rad), std::cos(heading_rad)};
    const auto forward_alignment = cross(gate_vector, expected_direction) / gate_length_m;
    if (!std::isfinite(decision.signed_start_distance_m) ||
        !std::isfinite(decision.signed_end_distance_m) ||
        !std::isfinite(forward_alignment) ||
        std::abs(forward_alignment) <= kMinimumForwardAlignment) {
        return decision;
    }

    const auto orientation = forward_alignment > 0.0 ? 1.0 : -1.0;
    if (decision.signed_start_distance_m * orientation >= -kLineSideToleranceM ||
        decision.signed_end_distance_m * orientation < -kLineSideToleranceM) {
        decision.result = CrossingValidationResult::wrong_direction;
        return decision;
    }

    const auto fraction = intersection.movement_fraction;
    decision.crossing_speed_mps = static_cast<float>(
        fix_0.speed_mps + fraction * (fix_1.speed_mps - fix_0.speed_mps));
    if (decision.crossing_speed_mps < gate.minimum_crossing_speed_mps) {
        decision.result = CrossingValidationResult::below_minimum_speed;
        return decision;
    }

    decision.crossing_heading_deg =
        interpolate_heading_deg(fix_0.heading_deg, fix_1.heading_deg, fraction);
    decision.heading_difference_deg = circular_heading_difference_deg(
        decision.crossing_heading_deg,
        static_cast<float>(gate.direction_heading_deg));
    if (decision.heading_difference_deg > gate.heading_tolerance_deg) {
        decision.result = CrossingValidationResult::heading_outside_tolerance;
        return decision;
    }

    decision.result = CrossingValidationResult::accepted;
    return decision;
}

float circular_heading_difference_deg(const float lhs_deg,
                                      const float rhs_deg) noexcept
{
    if (!std::isfinite(lhs_deg) || !std::isfinite(rhs_deg)) {
        return 180.0F;
    }
    return std::abs(std::remainder(lhs_deg - rhs_deg, 360.0F));
}

const char* crossing_validation_result_name(
    const CrossingValidationResult result) noexcept
{
    switch (result) {
    case CrossingValidationResult::accepted:
        return "accepted";
    case CrossingValidationResult::invalid_configuration:
        return "invalid-configuration";
    case CrossingValidationResult::no_geometric_intersection:
        return "no-geometric-intersection";
    case CrossingValidationResult::source_fix_rejected:
        return "source-fix-rejected";
    case CrossingValidationResult::invalid_measurement:
        return "invalid-measurement";
    case CrossingValidationResult::insufficient_fix_type:
        return "insufficient-fix-type";
    case CrossingValidationResult::insufficient_satellites:
        return "insufficient-satellites";
    case CrossingValidationResult::excessive_horizontal_accuracy:
        return "excessive-horizontal-accuracy";
    case CrossingValidationResult::excessive_speed_accuracy:
        return "excessive-speed-accuracy";
    case CrossingValidationResult::excessive_heading_accuracy:
        return "excessive-heading-accuracy";
    case CrossingValidationResult::wrong_direction:
        return "wrong-direction";
    case CrossingValidationResult::below_minimum_speed:
        return "below-minimum-speed";
    case CrossingValidationResult::heading_outside_tolerance:
        return "heading-outside-tolerance";
    }
    return "invalid-configuration";
}

}  // namespace track_timer::timing
