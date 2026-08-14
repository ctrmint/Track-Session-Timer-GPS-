#pragma once

#include "track_timer/domain/contracts.hpp"

#include <cstdint>
#include <type_traits>

namespace track_timer::timing {

inline constexpr std::int64_t kGpsWeekNanoseconds = 604'800'000'000'000LL;
inline constexpr std::int64_t kDefaultMaximumFixPairGapNs = 500'000'000LL;
inline constexpr std::int64_t kDefaultMaximumFixAgeUs = 500'000LL;

enum class ReceiverTimeNormalizationResult : std::uint8_t {
    initialized,
    normalized,
    invalid_period,
    invalid_raw_time,
    non_monotonic,
    ambiguous_discontinuity,
    numeric_overflow,
};

struct ReceiverTimeNormalizer {
    std::int64_t last_raw_time_ns{0};
    std::int64_t last_normalized_time_ns{0};
    std::uint32_t rollover_count{0};
    bool initialized{false};
};

struct ReceiverTimeNormalization {
    ReceiverTimeNormalizationResult result{
        ReceiverTimeNormalizationResult::invalid_raw_time};
    std::int64_t normalized_time_ns{domain::kUnavailableTime};
    std::uint32_t rollover_count{0};
};

[[nodiscard]] ReceiverTimeNormalization normalize_receiver_time(
    std::int64_t raw_time_ns, ReceiverTimeNormalizer& state,
    std::int64_t period_ns = kGpsWeekNanoseconds) noexcept;

enum class CrossingTimeResult : std::uint8_t {
    interpolated,
    invalid_policy,
    invalid_fraction,
    unavailable_timestamp,
    non_monotonic_sequence,
    non_monotonic_measurement_time,
    non_monotonic_arrival_time,
    stale_fix,
    excessive_pair_gap,
};

struct CrossingTimePolicy {
    std::int64_t maximum_fix_pair_gap_ns{kDefaultMaximumFixPairGapNs};
    std::int64_t maximum_fix_age_us{kDefaultMaximumFixAgeUs};
};

struct CrossingTimeDecision {
    CrossingTimeResult result{CrossingTimeResult::invalid_policy};
    std::int64_t crossing_measurement_time_ns{domain::kUnavailableTime};
    std::int64_t pair_interval_ns{0};
    std::int64_t current_fix_age_us{0};
    std::uint32_t segment_sequence_0{0};
    std::uint32_t segment_sequence_1{0};
    double intersection_fraction{0.0};
    CrossingTimePolicy policy{};
};

[[nodiscard]] CrossingTimeDecision interpolate_crossing_measurement_time(
    const domain::GnssFix& fix_0, const domain::GnssFix& fix_1,
    double intersection_fraction, std::int64_t evaluation_monotonic_us,
    const CrossingTimePolicy& policy = {}) noexcept;

[[nodiscard]] const char* receiver_time_normalization_result_name(
    ReceiverTimeNormalizationResult result) noexcept;
[[nodiscard]] const char* crossing_time_result_name(CrossingTimeResult result) noexcept;

static_assert(std::is_trivially_copyable_v<ReceiverTimeNormalizer>);
static_assert(std::is_trivially_copyable_v<ReceiverTimeNormalization>);
static_assert(std::is_trivially_copyable_v<CrossingTimePolicy>);
static_assert(std::is_trivially_copyable_v<CrossingTimeDecision>);
static_assert(sizeof(CrossingTimeDecision) <= 64);

}  // namespace track_timer::timing
