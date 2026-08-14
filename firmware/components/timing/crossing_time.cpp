#include "track_timer/timing/crossing_time.hpp"

#include <cmath>
#include <limits>

namespace track_timer::timing {

ReceiverTimeNormalization normalize_receiver_time(
    const std::int64_t raw_time_ns, ReceiverTimeNormalizer& state,
    const std::int64_t period_ns) noexcept
{
    if (period_ns < 2) {
        return {ReceiverTimeNormalizationResult::invalid_period};
    }
    if (raw_time_ns < 0 || raw_time_ns >= period_ns) {
        return {ReceiverTimeNormalizationResult::invalid_raw_time};
    }
    if (!state.initialized) {
        state.last_raw_time_ns = raw_time_ns;
        state.last_normalized_time_ns = raw_time_ns;
        state.rollover_count = 0;
        state.initialized = true;
        return {ReceiverTimeNormalizationResult::initialized, raw_time_ns, 0};
    }

    auto delta_ns = raw_time_ns - state.last_raw_time_ns;
    bool rolled_over = false;
    const auto half_period_ns = period_ns / 2;
    if (delta_ns < 0) {
        if (-delta_ns < half_period_ns) {
            return {ReceiverTimeNormalizationResult::non_monotonic,
                    domain::kUnavailableTime, state.rollover_count};
        }
        if (-delta_ns == half_period_ns) {
            return {ReceiverTimeNormalizationResult::ambiguous_discontinuity,
                    domain::kUnavailableTime, state.rollover_count};
        }
        delta_ns += period_ns;
        rolled_over = true;
    }
    else if (delta_ns == 0) {
        return {ReceiverTimeNormalizationResult::non_monotonic,
                domain::kUnavailableTime, state.rollover_count};
    }
    else if (delta_ns >= half_period_ns) {
        return {ReceiverTimeNormalizationResult::ambiguous_discontinuity,
                domain::kUnavailableTime, state.rollover_count};
    }

    if (state.last_normalized_time_ns >
            std::numeric_limits<std::int64_t>::max() - delta_ns ||
        (rolled_over &&
         state.rollover_count == std::numeric_limits<std::uint32_t>::max())) {
        return {ReceiverTimeNormalizationResult::numeric_overflow,
                domain::kUnavailableTime, state.rollover_count};
    }

    const auto normalized_time_ns = state.last_normalized_time_ns + delta_ns;
    state.last_raw_time_ns = raw_time_ns;
    state.last_normalized_time_ns = normalized_time_ns;
    if (rolled_over) {
        ++state.rollover_count;
    }
    return {ReceiverTimeNormalizationResult::normalized, normalized_time_ns,
            state.rollover_count};
}

CrossingTimeDecision interpolate_crossing_measurement_time(
    const domain::GnssFix& fix_0, const domain::GnssFix& fix_1,
    const double intersection_fraction, const std::int64_t evaluation_monotonic_us,
    const CrossingTimePolicy& policy) noexcept
{
    CrossingTimeDecision decision{};
    decision.segment_sequence_0 = fix_0.sequence_number;
    decision.segment_sequence_1 = fix_1.sequence_number;
    decision.intersection_fraction = intersection_fraction;
    decision.policy = policy;

    if (policy.maximum_fix_pair_gap_ns <= 0 || policy.maximum_fix_age_us <= 0) {
        return decision;
    }
    if (!std::isfinite(intersection_fraction) || intersection_fraction <= 0.0 ||
        intersection_fraction > 1.0) {
        decision.result = CrossingTimeResult::invalid_fraction;
        return decision;
    }
    if (fix_0.measurement_time_ns < 0 || fix_1.measurement_time_ns < 0 ||
        fix_0.arrival_monotonic_us < 0 || fix_1.arrival_monotonic_us < 0 ||
        evaluation_monotonic_us < 0) {
        decision.result = CrossingTimeResult::unavailable_timestamp;
        return decision;
    }
    if (fix_1.sequence_number <= fix_0.sequence_number) {
        decision.result = CrossingTimeResult::non_monotonic_sequence;
        return decision;
    }
    if (fix_1.measurement_time_ns <= fix_0.measurement_time_ns) {
        decision.result = CrossingTimeResult::non_monotonic_measurement_time;
        return decision;
    }
    if (fix_1.arrival_monotonic_us <= fix_0.arrival_monotonic_us ||
        evaluation_monotonic_us < fix_1.arrival_monotonic_us) {
        decision.result = CrossingTimeResult::non_monotonic_arrival_time;
        return decision;
    }

    decision.current_fix_age_us =
        evaluation_monotonic_us - fix_1.arrival_monotonic_us;
    if (decision.current_fix_age_us > policy.maximum_fix_age_us) {
        decision.result = CrossingTimeResult::stale_fix;
        return decision;
    }

    decision.pair_interval_ns =
        fix_1.measurement_time_ns - fix_0.measurement_time_ns;
    if (decision.pair_interval_ns > policy.maximum_fix_pair_gap_ns) {
        decision.result = CrossingTimeResult::excessive_pair_gap;
        return decision;
    }

    const auto fractional_interval_ns =
        static_cast<long double>(intersection_fraction) *
        static_cast<long double>(decision.pair_interval_ns);
    const auto rounded_interval_ns = static_cast<std::int64_t>(
        std::floor(fractional_interval_ns + 0.5L));
    decision.crossing_measurement_time_ns =
        fix_0.measurement_time_ns + rounded_interval_ns;
    decision.result = CrossingTimeResult::interpolated;
    return decision;
}

const char* receiver_time_normalization_result_name(
    const ReceiverTimeNormalizationResult result) noexcept
{
    switch (result) {
    case ReceiverTimeNormalizationResult::initialized:
        return "initialized";
    case ReceiverTimeNormalizationResult::normalized:
        return "normalized";
    case ReceiverTimeNormalizationResult::invalid_period:
        return "invalid-period";
    case ReceiverTimeNormalizationResult::invalid_raw_time:
        return "invalid-raw-time";
    case ReceiverTimeNormalizationResult::non_monotonic:
        return "non-monotonic";
    case ReceiverTimeNormalizationResult::ambiguous_discontinuity:
        return "ambiguous-discontinuity";
    case ReceiverTimeNormalizationResult::numeric_overflow:
        return "numeric-overflow";
    }
    return "invalid-raw-time";
}

const char* crossing_time_result_name(const CrossingTimeResult result) noexcept
{
    switch (result) {
    case CrossingTimeResult::interpolated:
        return "interpolated";
    case CrossingTimeResult::invalid_policy:
        return "invalid-policy";
    case CrossingTimeResult::invalid_fraction:
        return "invalid-fraction";
    case CrossingTimeResult::unavailable_timestamp:
        return "unavailable-timestamp";
    case CrossingTimeResult::non_monotonic_sequence:
        return "non-monotonic-sequence";
    case CrossingTimeResult::non_monotonic_measurement_time:
        return "non-monotonic-measurement-time";
    case CrossingTimeResult::non_monotonic_arrival_time:
        return "non-monotonic-arrival-time";
    case CrossingTimeResult::stale_fix:
        return "stale-fix";
    case CrossingTimeResult::excessive_pair_gap:
        return "excessive-pair-gap";
    }
    return "invalid-policy";
}

}  // namespace track_timer::timing
