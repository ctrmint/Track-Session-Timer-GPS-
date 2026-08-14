#include "track_timer/timing/crossing_time.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>

namespace {

track_timer::domain::GnssFix fix(const std::uint32_t sequence,
                                 const std::int64_t measurement_time_ns,
                                 const std::int64_t arrival_monotonic_us)
{
    track_timer::domain::GnssFix value{};
    value.sequence_number = sequence;
    value.measurement_time_ns = measurement_time_ns;
    value.arrival_monotonic_us = arrival_monotonic_us;
    return value;
}

void expect(const track_timer::timing::CrossingTimeDecision& decision,
            const track_timer::timing::CrossingTimeResult result)
{
    assert(decision.result == result);
}

}  // namespace

int main()
{
    using namespace track_timer::timing;

    const auto nominal = interpolate_crossing_measurement_time(
        fix(10, 1'000'000'000, 1'000'000),
        fix(11, 1'040'000'000, 1'040'000), 0.25, 1'050'000);
    expect(nominal, CrossingTimeResult::interpolated);
    assert(nominal.crossing_measurement_time_ns == 1'010'000'000);
    assert(nominal.pair_interval_ns == 40'000'000);
    assert(nominal.current_fix_age_us == 10'000);
    assert(nominal.segment_sequence_0 == 10);
    assert(nominal.segment_sequence_1 == 11);

    const auto later_uart_arrival = interpolate_crossing_measurement_time(
        fix(10, 1'000'000'000, 5'000'000),
        fix(11, 1'040'000'000, 5'070'000), 0.25, 5'080'000);
    expect(later_uart_arrival, CrossingTimeResult::interpolated);
    assert(later_uart_arrival.crossing_measurement_time_ns ==
           nominal.crossing_measurement_time_ns);

    const auto fractional_nanosecond = interpolate_crossing_measurement_time(
        fix(1, 100, 1'000), fix(2, 103, 2'000), 0.5, 2'001);
    expect(fractional_nanosecond, CrossingTimeResult::interpolated);
    assert(fractional_nanosecond.crossing_measurement_time_ns == 102);

    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto near_limit = interpolate_crossing_measurement_time(
        fix(1, maximum - 20, 1'000), fix(2, maximum - 10, 2'000),
        0.5, 2'001);
    expect(near_limit, CrossingTimeResult::interpolated);
    assert(near_limit.crossing_measurement_time_ns == maximum - 15);

    expect(interpolate_crossing_measurement_time(
               fix(1, 100, 1'000), fix(2, 200, 2'000), 0.0, 2'001),
           CrossingTimeResult::invalid_fraction);
    expect(interpolate_crossing_measurement_time(
               fix(1, 100, 1'000), fix(2, 200, 2'000), 1.01, 2'001),
           CrossingTimeResult::invalid_fraction);

    auto unavailable = fix(1, track_timer::domain::kUnavailableTime, 1'000);
    expect(interpolate_crossing_measurement_time(
               unavailable, fix(2, 200, 2'000), 0.5, 2'001),
           CrossingTimeResult::unavailable_timestamp);
    expect(interpolate_crossing_measurement_time(
               fix(2, 100, 1'000), fix(2, 200, 2'000), 0.5, 2'001),
           CrossingTimeResult::non_monotonic_sequence);
    expect(interpolate_crossing_measurement_time(
               fix(1, 100, 1'000), fix(2, 100, 2'000), 0.5, 2'001),
           CrossingTimeResult::non_monotonic_measurement_time);
    expect(interpolate_crossing_measurement_time(
               fix(1, 200, 1'000), fix(2, 100, 2'000), 0.5, 2'001),
           CrossingTimeResult::non_monotonic_measurement_time);
    expect(interpolate_crossing_measurement_time(
               fix(1, 100, 2'000), fix(2, 200, 2'000), 0.5, 2'001),
           CrossingTimeResult::non_monotonic_arrival_time);

    CrossingTimePolicy short_age{};
    short_age.maximum_fix_age_us = 100;
    expect(interpolate_crossing_measurement_time(
               fix(1, 100, 1'000), fix(2, 200, 2'000), 0.5, 2'101,
               short_age),
           CrossingTimeResult::stale_fix);
    CrossingTimePolicy short_gap{};
    short_gap.maximum_fix_pair_gap_ns = 50;
    expect(interpolate_crossing_measurement_time(
               fix(1, 100, 1'000), fix(2, 200, 2'000), 0.5, 2'001,
               short_gap),
           CrossingTimeResult::excessive_pair_gap);
    CrossingTimePolicy invalid_policy{};
    invalid_policy.maximum_fix_pair_gap_ns = 0;
    expect(interpolate_crossing_measurement_time(
               fix(1, 100, 1'000), fix(2, 200, 2'000), 0.5, 2'001,
               invalid_policy),
           CrossingTimeResult::invalid_policy);

    ReceiverTimeNormalizer week{};
    const auto first = normalize_receiver_time(kGpsWeekNanoseconds - 20'000'000,
                                               week);
    assert(first.result == ReceiverTimeNormalizationResult::initialized);
    const auto rollover = normalize_receiver_time(20'000'000, week);
    assert(rollover.result == ReceiverTimeNormalizationResult::normalized);
    assert(rollover.normalized_time_ns == kGpsWeekNanoseconds + 20'000'000);
    assert(rollover.rollover_count == 1);

    const auto after_rollover = normalize_receiver_time(60'000'000, week);
    assert(after_rollover.result == ReceiverTimeNormalizationResult::normalized);
    assert(after_rollover.normalized_time_ns == kGpsWeekNanoseconds + 60'000'000);

    ReceiverTimeNormalizer reverse{};
    assert(normalize_receiver_time(100, reverse, 1'000).result ==
           ReceiverTimeNormalizationResult::initialized);
    assert(normalize_receiver_time(90, reverse, 1'000).result ==
           ReceiverTimeNormalizationResult::non_monotonic);
    assert(reverse.last_raw_time_ns == 100);

    ReceiverTimeNormalizer discontinuity{};
    assert(normalize_receiver_time(10, discontinuity, 1'000).result ==
           ReceiverTimeNormalizationResult::initialized);
    assert(normalize_receiver_time(900, discontinuity, 1'000).result ==
           ReceiverTimeNormalizationResult::ambiguous_discontinuity);
    assert(discontinuity.last_raw_time_ns == 10);

    ReceiverTimeNormalizer half_period{};
    assert(normalize_receiver_time(100, half_period, 1'000).result ==
           ReceiverTimeNormalizationResult::initialized);
    assert(normalize_receiver_time(600, half_period, 1'000).result ==
           ReceiverTimeNormalizationResult::ambiguous_discontinuity);

    ReceiverTimeNormalizer overflow{};
    overflow.last_raw_time_ns = 100;
    overflow.last_normalized_time_ns = maximum - 10;
    overflow.initialized = true;
    assert(normalize_receiver_time(120, overflow, 1'000).result ==
           ReceiverTimeNormalizationResult::numeric_overflow);
    assert(overflow.last_normalized_time_ns == maximum - 10);

    ReceiverTimeNormalizer invalid_raw{};
    assert(normalize_receiver_time(-1, invalid_raw, 1'000).result ==
           ReceiverTimeNormalizationResult::invalid_raw_time);
    assert(normalize_receiver_time(1'000, invalid_raw, 1'000).result ==
           ReceiverTimeNormalizationResult::invalid_raw_time);
    assert(normalize_receiver_time(0, invalid_raw, 1).result ==
           ReceiverTimeNormalizationResult::invalid_period);

    const std::array normalization_results{
        ReceiverTimeNormalizationResult::initialized,
        ReceiverTimeNormalizationResult::normalized,
        ReceiverTimeNormalizationResult::invalid_period,
        ReceiverTimeNormalizationResult::invalid_raw_time,
        ReceiverTimeNormalizationResult::non_monotonic,
        ReceiverTimeNormalizationResult::ambiguous_discontinuity,
        ReceiverTimeNormalizationResult::numeric_overflow,
    };
    for (const auto result : normalization_results) {
        assert(receiver_time_normalization_result_name(result)[0] != '\0');
    }
    const std::array crossing_results{
        CrossingTimeResult::interpolated,
        CrossingTimeResult::invalid_policy,
        CrossingTimeResult::invalid_fraction,
        CrossingTimeResult::unavailable_timestamp,
        CrossingTimeResult::non_monotonic_sequence,
        CrossingTimeResult::non_monotonic_measurement_time,
        CrossingTimeResult::non_monotonic_arrival_time,
        CrossingTimeResult::stale_fix,
        CrossingTimeResult::excessive_pair_gap,
    };
    for (const auto result : crossing_results) {
        assert(crossing_time_result_name(result)[0] != '\0');
    }

    std::cout << "Receiver-time rollover, ordering, and crossing interpolation passed\n";
    return 0;
}
