#pragma once

#include "track_timer/domain/contracts.hpp"
#include "track_timer/timing/intersection.hpp"
#include "track_timer/track/definition.hpp"

#include <cstdint>
#include <type_traits>

namespace track_timer::timing {

inline constexpr float kDefaultMaximumHorizontalAccuracyM = 5.0F;
inline constexpr float kDefaultMaximumSpeedAccuracyMps = 2.0F;
inline constexpr float kDefaultMaximumHeadingAccuracyDeg = 25.0F;
inline constexpr std::uint16_t kDefaultMinimumSatellites = 6;

enum class CrossingFixRequirement : std::uint8_t {
    two_dimensional,
    three_dimensional,
};

struct CrossingQualityThresholds {
    float maximum_horizontal_accuracy_m{kDefaultMaximumHorizontalAccuracyM};
    float maximum_speed_accuracy_mps{kDefaultMaximumSpeedAccuracyMps};
    float maximum_heading_accuracy_deg{kDefaultMaximumHeadingAccuracyDeg};
    std::uint16_t minimum_satellites{kDefaultMinimumSatellites};
    CrossingFixRequirement minimum_fix{CrossingFixRequirement::three_dimensional};
};

enum class CrossingValidationResult : std::uint8_t {
    accepted,
    invalid_configuration,
    no_geometric_intersection,
    source_fix_rejected,
    invalid_measurement,
    insufficient_fix_type,
    insufficient_satellites,
    excessive_horizontal_accuracy,
    excessive_speed_accuracy,
    excessive_heading_accuracy,
    wrong_direction,
    below_minimum_speed,
    heading_outside_tolerance,
};

struct CrossingValidationDecision {
    CrossingValidationResult result{CrossingValidationResult::invalid_configuration};
    std::uint32_t segment_sequence_0{0};
    std::uint32_t segment_sequence_1{0};
    double intersection_fraction{0.0};
    double signed_start_distance_m{0.0};
    double signed_end_distance_m{0.0};
    float crossing_speed_mps{0.0F};
    float crossing_heading_deg{0.0F};
    float heading_difference_deg{0.0F};
    CrossingQualityThresholds thresholds{};
};

[[nodiscard]] CrossingValidationDecision validate_crossing(
    const domain::GnssFix& fix_0, const track::LocalPoint& position_0,
    const domain::GnssFix& fix_1, const track::LocalPoint& position_1,
    const track::DirectedGateDefinition& gate,
    const SegmentIntersection& intersection,
    const CrossingQualityThresholds& thresholds = {}) noexcept;

[[nodiscard]] float circular_heading_difference_deg(float lhs_deg,
                                                     float rhs_deg) noexcept;
[[nodiscard]] const char* crossing_validation_result_name(
    CrossingValidationResult result) noexcept;

static_assert(std::is_trivially_copyable_v<CrossingQualityThresholds>);
static_assert(std::is_trivially_copyable_v<CrossingValidationDecision>);
static_assert(sizeof(CrossingValidationDecision) <= 80);

}  // namespace track_timer::timing
