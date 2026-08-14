#include "track_timer/timing/engine.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace {

using track_timer::domain::GnssFix;
using track_timer::timing::GateCrossingDecision;
using track_timer::timing::GateCrossingResult;
using track_timer::timing::LapBoundary;
using track_timer::timing::TimingEngine;
using track_timer::timing::TimingEngineConfig;
using track_timer::timing::TimingGate;
using track_timer::track::CircuitProjection;
using track_timer::track::GeographicPoint;
using track_timer::track::LocalPoint;

constexpr double kPi = 3.14159265358979323846;
constexpr std::int64_t kBaseTimeNs = 1'000'000'000LL;

GeographicPoint geographic(const CircuitProjection& projection,
                           const LocalPoint& local) noexcept
{
    return {
        projection.reference.latitude_deg +
            local.north_m / projection.north_metres_per_radian * 180.0 / kPi,
        projection.reference.longitude_deg +
            local.east_m / projection.east_metres_per_radian * 180.0 / kPi,
    };
}

track_timer::track::DirectedGateDefinition gate(
    const CircuitProjection& projection, const double east_m,
    const double minimum_speed_mps = 2.0, const double corridor_m = 1.0)
{
    track_timer::track::DirectedGateDefinition value{};
    value.left = geographic(projection, {east_m, 10.0});
    value.right = geographic(projection, {east_m, -10.0});
    value.direction_heading_deg = 90.0;
    value.heading_tolerance_deg = 20.0;
    value.minimum_crossing_speed_mps = minimum_speed_mps;
    value.rearm_corridor_m = corridor_m;
    return value;
}

TimingEngineConfig configuration(CircuitProjection& projection,
                                 const LapBoundary boundary = LapBoundary::finish)
{
    TimingEngineConfig config{};
    config.reference = {52.0, -1.0};
    assert(track_timer::track::configure_circuit_projection(config.reference,
                                                             projection) ==
           track_timer::track::ProjectionResult::projected);
    config.gates.start = gate(projection, 0.0);
    config.gates.finish = gate(projection, 20.0);
    config.gates.pit_exit = gate(projection, 40.0, 0.5);
    config.gates.pit_entry = gate(projection, 60.0, 0.5);
    config.lap_boundary = boundary;
    config.minimum_lap_time_s = 1.0;
    return config;
}

GnssFix fix(const CircuitProjection& projection, const LocalPoint& local,
            const std::uint32_t sequence, const std::int64_t period_ns = 40'000'000,
            const float speed_mps = 100.0F, const float heading_deg = 90.0F)
{
    const auto point = geographic(projection, local);
    GnssFix value{};
    value.measurement_time_ns = kBaseTimeNs + sequence * period_ns;
    value.arrival_monotonic_us = value.measurement_time_ns / 1'000 + 1'000;
    value.latitude_deg = point.latitude_deg;
    value.longitude_deg = point.longitude_deg;
    value.speed_mps = speed_mps;
    value.heading_deg = heading_deg;
    value.horizontal_accuracy_m = 0.8F;
    value.speed_accuracy_mps = 0.2F;
    value.heading_accuracy_deg = 2.0F;
    value.sequence_number = sequence;
    value.num_satellites = 12;
    value.fix_type = track_timer::domain::FixType::fix_3d;
    value.accepted_for_timing = true;
    return value;
}

const GateCrossingDecision& decision_for(
    const track_timer::timing::TimingEngineDecision& decision,
    const TimingGate gate_type)
{
    return decision.gate_decisions[static_cast<std::size_t>(gate_type)];
}

void prime(TimingEngine& engine, const GnssFix& initial)
{
    assert(engine.process_fix(initial, initial.arrival_monotonic_us).result ==
           track_timer::timing::TimingEngineResult::primed);
}

void assert_same_gate_decisions(
    const track_timer::timing::TimingEngineDecision& lhs,
    const track_timer::timing::TimingEngineDecision& rhs)
{
    for (std::size_t index = 0; index < lhs.gate_decisions.size(); ++index) {
        const auto& left = lhs.gate_decisions[index];
        const auto& right = rhs.gate_decisions[index];
        assert(left.crossing_measurement_time_ns == right.crossing_measurement_time_ns);
        assert(left.intersection_fraction == right.intersection_fraction);
        assert(left.segment_sequence_0 == right.segment_sequence_0);
        assert(left.segment_sequence_1 == right.segment_sequence_1);
        assert(left.gate == right.gate);
        assert(left.result == right.result);
        assert(left.intersection_result == right.intersection_result);
        assert(left.validation_result == right.validation_result);
        assert(left.time_result == right.time_result);
        assert(left.has_event == right.has_event);
    }
}

}  // namespace

int main()
{
    using namespace track_timer::timing;

    CircuitProjection projection{};
    const auto finish_config = configuration(projection);
    TimingEngine finish_engine{};
    assert(finish_engine.configure(finish_config) ==
           TimingEngineConfigureResult::configured);
    auto initial = fix(projection, {-10.0, 0.0}, 0);
    auto high_speed = fix(projection, {70.0, 0.0}, 1);
    prime(finish_engine, initial);
    const auto all_gates = finish_engine.process_fix(
        high_speed, high_speed.arrival_monotonic_us);
    const std::array expected_fractions{0.125, 0.375, 0.875, 0.625};
    for (std::size_t index = 0; index < all_gates.gate_decisions.size(); ++index) {
        const auto& gate_decision = all_gates.gate_decisions[index];
        assert(gate_decision.result == GateCrossingResult::event);
        assert(gate_decision.has_event);
        assert(std::abs(gate_decision.intersection_fraction - expected_fractions[index]) <
               1.0e-9);
        assert(gate_decision.crossing_measurement_time_ns ==
               kBaseTimeNs + static_cast<std::int64_t>(std::llround(
                                 expected_fractions[index] * 40'000'000)));
    }
    assert(finish_engine.lap_snapshot().current_lap_started_ns ==
           decision_for(all_gates, TimingGate::finish).crossing_measurement_time_ns);

    // Selecting start changes only which already-typed event feeds lap state.
    CircuitProjection start_projection{};
    const auto start_config = configuration(start_projection, LapBoundary::start);
    TimingEngine start_engine{};
    assert(start_engine.configure(start_config) == TimingEngineConfigureResult::configured);
    prime(start_engine, fix(start_projection, {-10.0, 0.0}, 0));
    const auto start_selected = start_engine.process_fix(
        fix(start_projection, {70.0, 0.0}, 1), high_speed.arrival_monotonic_us + 5'000);
    assert_same_gate_decisions(all_gates, start_selected);
    assert(start_engine.lap_snapshot().current_lap_started_ns ==
           decision_for(start_selected, TimingGate::start).crossing_measurement_time_ns);
    assert(start_engine.lap_snapshot().current_lap_started_ns !=
           finish_engine.lap_snapshot().current_lap_started_ns);

    // A 20 Hz, large-step sample still intersects the finite gate and interpolates time.
    TimingEngine twenty_hz{};
    assert(twenty_hz.configure(finish_config) == TimingEngineConfigureResult::configured);
    auto twenty_start = fix(projection, {10.0, 0.0}, 0, 50'000'000);
    auto twenty_end = fix(projection, {30.0, 0.0}, 1, 50'000'000);
    prime(twenty_hz, twenty_start);
    const auto twenty_crossing = twenty_hz.process_fix(
        twenty_end, twenty_end.arrival_monotonic_us);
    const auto& twenty_finish = decision_for(twenty_crossing, TimingGate::finish);
    assert(twenty_finish.result == GateCrossingResult::event);
    assert(std::abs(twenty_finish.intersection_fraction - 0.5) < 1.0e-9);
    assert(twenty_finish.crossing_measurement_time_ns == kBaseTimeNs + 25'000'000);

    // Immediate on-line jitter cannot emit a second forward event before corridor exit.
    CircuitProjection jitter_projection{};
    auto jitter_config = configuration(jitter_projection, LapBoundary::start);
    TimingEngine jitter{};
    assert(jitter.configure(jitter_config) == TimingEngineConfigureResult::configured);
    prime(jitter, fix(jitter_projection, {-2.0, 0.0}, 0));
    const auto first = jitter.process_fix(
        fix(jitter_projection, {0.1, 0.0}, 1), kBaseTimeNs / 1'000 + 50'000);
    assert(decision_for(first, TimingGate::start).has_event);
    const auto reverse = jitter.process_fix(
        fix(jitter_projection, {-0.1, 0.0}, 2, 40'000'000, 5.0F, 90.0F),
        kBaseTimeNs / 1'000 + 90'000);
    assert(decision_for(reverse, TimingGate::start).result ==
           GateCrossingResult::crossing_rejected);
    const auto repeated = jitter.process_fix(
        fix(jitter_projection, {0.1, 0.0}, 3, 40'000'000, 5.0F, 90.0F),
        kBaseTimeNs / 1'000 + 130'000);
    assert(decision_for(repeated, TimingGate::start).result ==
           GateCrossingResult::rearm_required);
    assert(!decision_for(repeated, TimingGate::start).has_event);

    // A fix on the line belongs to the preceding segment only; an endpoint is valid.
    TimingEngine on_line{};
    assert(on_line.configure(start_config) == TimingEngineConfigureResult::configured);
    prime(on_line, fix(start_projection, {-2.0, 0.0}, 0));
    const auto to_line = on_line.process_fix(
        fix(start_projection, {0.0, 0.0}, 1), kBaseTimeNs / 1'000 + 50'000);
    assert(decision_for(to_line, TimingGate::start).has_event);
    const auto from_line = on_line.process_fix(
        fix(start_projection, {2.0, 0.0}, 2), kBaseTimeNs / 1'000 + 90'000);
    assert(!decision_for(from_line, TimingGate::start).has_event);
    assert(decision_for(from_line, TimingGate::start).intersection_result ==
           SegmentIntersectionResult::movement_start_not_owned);

    TimingEngine endpoint{};
    assert(endpoint.configure(start_config) == TimingEngineConfigureResult::configured);
    prime(endpoint, fix(start_projection, {-2.0, 10.0}, 0));
    const auto endpoint_crossing = endpoint.process_fix(
        fix(start_projection, {2.0, 10.0}, 1), kBaseTimeNs / 1'000 + 50'000);
    assert(decision_for(endpoint_crossing, TimingGate::start).has_event);

    TimingEngine parallel{};
    assert(parallel.configure(start_config) == TimingEngineConfigureResult::configured);
    prime(parallel, fix(start_projection, {-1.0, -5.0}, 0, 40'000'000, 5.0F, 0.0F));
    const auto parallel_move = parallel.process_fix(
        fix(start_projection, {-1.0, 5.0}, 1, 40'000'000, 5.0F, 0.0F),
        kBaseTimeNs / 1'000 + 50'000);
    assert(decision_for(parallel_move, TimingGate::start).intersection_result ==
           SegmentIntersectionResult::parallel);
    assert(!decision_for(parallel_move, TimingGate::start).has_event);

    // Slow pit-lane movement is accepted by its own threshold.
    TimingEngine slow_pit{};
    assert(slow_pit.configure(finish_config) == TimingEngineConfigureResult::configured);
    prime(slow_pit, fix(projection, {35.0, 0.0}, 0, 40'000'000, 1.5F));
    const auto pit_crossing = slow_pit.process_fix(
        fix(projection, {45.0, 0.0}, 1, 40'000'000, 1.5F),
        kBaseTimeNs / 1'000 + 50'000);
    assert(decision_for(pit_crossing, TimingGate::pit_exit).has_event);

    // Reverse, poor-quality, stale, and non-monotonic pairs remain replayable rejects.
    TimingEngine reverse_engine{};
    assert(reverse_engine.configure(start_config) == TimingEngineConfigureResult::configured);
    prime(reverse_engine, fix(start_projection, {2.0, 0.0}, 0, 40'000'000, 5.0F,
                              270.0F));
    const auto reverse_crossing = reverse_engine.process_fix(
        fix(start_projection, {-2.0, 0.0}, 1, 40'000'000, 5.0F, 270.0F),
        kBaseTimeNs / 1'000 + 50'000);
    assert(decision_for(reverse_crossing, TimingGate::start).validation_result ==
           CrossingValidationResult::wrong_direction);

    TimingEngine poor_engine{};
    assert(poor_engine.configure(start_config) == TimingEngineConfigureResult::configured);
    auto poor_start = fix(start_projection, {-2.0, 0.0}, 0);
    auto poor_end = fix(start_projection, {2.0, 0.0}, 1);
    poor_end.horizontal_accuracy_m = 8.0F;
    prime(poor_engine, poor_start);
    const auto poor_crossing = poor_engine.process_fix(
        poor_end, poor_end.arrival_monotonic_us);
    assert(decision_for(poor_crossing, TimingGate::start).validation_result ==
           CrossingValidationResult::excessive_horizontal_accuracy);

    TimingEngine stale_engine{};
    assert(stale_engine.configure(start_config) == TimingEngineConfigureResult::configured);
    prime(stale_engine, fix(start_projection, {-2.0, 0.0}, 0));
    auto stale_end = fix(start_projection, {2.0, 0.0}, 1);
    const auto stale = stale_engine.process_fix(
        stale_end, stale_end.arrival_monotonic_us + 600'000);
    assert(decision_for(stale, TimingGate::start).result ==
           GateCrossingResult::timestamp_rejected);
    assert(decision_for(stale, TimingGate::start).time_result ==
           CrossingTimeResult::stale_fix);

    TimingEngine sequence_engine{};
    assert(sequence_engine.configure(start_config) == TimingEngineConfigureResult::configured);
    prime(sequence_engine, fix(start_projection, {-2.0, 0.0}, 2));
    const auto bad_sequence = sequence_engine.process_fix(
        fix(start_projection, {2.0, 0.0}, 1), kBaseTimeNs / 1'000 + 100'000);
    assert(decision_for(bad_sequence, TimingGate::start).time_result ==
           CrossingTimeResult::non_monotonic_sequence);

    for (const auto gate_type : {TimingGate::start, TimingGate::finish,
                                 TimingGate::pit_entry, TimingGate::pit_exit}) {
        assert(std::strlen(timing_gate_name(gate_type)) > 0);
    }
    for (const auto result : {GateCrossingResult::not_evaluated,
                              GateCrossingResult::no_intersection,
                              GateCrossingResult::crossing_rejected,
                              GateCrossingResult::timestamp_rejected,
                              GateCrossingResult::rearm_required,
                              GateCrossingResult::event}) {
        assert(std::strlen(gate_crossing_result_name(result)) > 0);
    }

    std::cout << "Typed four-gate events, independent rearm, interpolation, and rejection replay passed\n";
}
