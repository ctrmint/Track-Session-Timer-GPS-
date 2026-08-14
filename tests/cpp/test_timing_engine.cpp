#include "track_timer/timing/embedded_runtime.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

using track_timer::domain::GnssFix;
using track_timer::domain::LapEvent;
using track_timer::timing::TimingEngine;
using track_timer::timing::TimingEngineConfig;
using track_timer::timing::TimingEngineConfigureResult;
using track_timer::track::CircuitProjection;
using track_timer::track::GeographicPoint;
using track_timer::track::LocalPoint;

constexpr double kPi = 3.14159265358979323846;
constexpr std::int64_t kFixPeriodNs = 40'000'000;
constexpr double kStepM = 0.2;

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

GnssFix make_fix(const CircuitProjection& projection, const LocalPoint& local,
                 const float heading_deg, const std::uint32_t sequence) noexcept
{
    const auto point = geographic(projection, local);
    GnssFix fix{};
    fix.measurement_time_ns = 1'000'000'000LL + sequence * kFixPeriodNs;
    fix.arrival_monotonic_us = fix.measurement_time_ns / 1'000 + 1'000;
    fix.latitude_deg = point.latitude_deg;
    fix.longitude_deg = point.longitude_deg;
    fix.speed_mps = 5.0F;
    fix.heading_deg = heading_deg;
    fix.horizontal_accuracy_m = 0.8F;
    fix.speed_accuracy_mps = 0.2F;
    fix.heading_accuracy_deg = 2.0F;
    fix.sequence_number = sequence;
    fix.num_satellites = 12;
    fix.fix_type = track_timer::domain::FixType::fix_3d;
    fix.accepted_for_timing = true;
    return fix;
}

void append_segment(std::vector<GnssFix>& fixes, const CircuitProjection& projection,
                    LocalPoint& current, const LocalPoint target,
                    const float heading_deg)
{
    const auto east_delta = target.east_m - current.east_m;
    const auto north_delta = target.north_m - current.north_m;
    const auto steps = static_cast<std::size_t>(
        std::ceil(std::hypot(east_delta, north_delta) / kStepM));
    const auto start = current;
    for (std::size_t step = 1; step <= steps; ++step) {
        const auto fraction = static_cast<double>(step) / static_cast<double>(steps);
        current = {start.east_m + fraction * east_delta,
                   start.north_m + fraction * north_delta};
        fixes.push_back(make_fix(projection, current, heading_deg,
                                 static_cast<std::uint32_t>(fixes.size())));
    }
}

TimingEngineConfig configuration(CircuitProjection& projection)
{
    TimingEngineConfig config{};
    config.reference = {52.0, -1.0};
    assert(track_timer::track::configure_circuit_projection(config.reference,
                                                             projection) ==
           track_timer::track::ProjectionResult::projected);
    config.gates.finish.left = geographic(projection, {0.0, 10.0});
    config.gates.finish.right = geographic(projection, {0.0, -10.0});
    config.gates.finish.direction_heading_deg = 90.0;
    config.gates.finish.heading_tolerance_deg = 20.0;
    config.gates.finish.minimum_crossing_speed_mps = 1.0;
    config.gates.finish.rearm_corridor_m = 2.0;
    config.gates.start = config.gates.finish;
    config.gates.pit_entry = config.gates.finish;
    config.gates.pit_exit = config.gates.finish;
    config.minimum_lap_time_s = 8.0;
    return config;
}

std::vector<GnssFix> walking_fixture(const CircuitProjection& projection)
{
    std::vector<GnssFix> fixes{};
    LocalPoint current{-3.0, 0.0};
    fixes.push_back(make_fix(projection, current, 90.0F, 0));
    append_segment(fixes, projection, current, {3.0, 0.0}, 90.0F);
    for (std::size_t lap = 0; lap < 3; ++lap) {
        append_segment(fixes, projection, current, {3.0, 15.0}, 0.0F);
        append_segment(fixes, projection, current, {-3.0, 15.0}, 270.0F);
        append_segment(fixes, projection, current, {-3.0, 0.0}, 180.0F);
        append_segment(fixes, projection, current, {3.0, 0.0}, 90.0F);
    }
    return fixes;
}

struct ReplayResult {
    std::array<LapEvent, track_timer::domain::queue_capacity::lap_events> events{};
    std::array<track_timer::timing::GateCrossingDecision,
               track_timer::domain::queue_capacity::gate_crossing_records>
        gate_records{};
    std::size_t event_count{0};
    std::size_t gate_record_count{0};
    std::int64_t maximum_processing_us{0};
};

ReplayResult replay(TimingEngine& engine, const std::vector<GnssFix>& fixes,
                    const bool add_arrival_jitter)
{
    ReplayResult result{};
    for (const auto& fix : fixes) {
        const auto started = std::chrono::steady_clock::now();
        const auto jitter_us = add_arrival_jitter
                                   ? static_cast<std::int64_t>(fix.sequence_number % 7U) *
                                         1'000
                                   : 0;
        const auto decision =
            engine.process_fix(fix, fix.arrival_monotonic_us + jitter_us);
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::steady_clock::now() - started)
                                 .count();
        result.maximum_processing_us = std::max(result.maximum_processing_us, elapsed);
        if (decision.lap_update.has_lap_event) {
            assert(result.event_count < result.events.size());
            result.events[result.event_count++] = decision.lap_update.lap_event;
        }
        for (const auto& gate_decision : decision.gate_decisions) {
            if (gate_decision.result ==
                    track_timer::timing::GateCrossingResult::not_evaluated ||
                gate_decision.result ==
                    track_timer::timing::GateCrossingResult::no_intersection) {
                continue;
            }
            assert(result.gate_record_count < result.gate_records.size());
            result.gate_records[result.gate_record_count++] = gate_decision;
        }
    }
    return result;
}

}  // namespace

int main()
{
    using namespace track_timer::timing;

    TimingEngine unconfigured{};
    GnssFix empty_fix{};
    assert(unconfigured.process_fix(empty_fix, 0).result ==
           TimingEngineResult::unconfigured);

    CircuitProjection projection{};
    auto config = configuration(projection);
    auto invalid_reference = config;
    invalid_reference.reference.latitude_deg = 90.0;
    assert(unconfigured.configure(invalid_reference) ==
           TimingEngineConfigureResult::invalid_reference);
    auto invalid_gate = config;
    invalid_gate.gates.pit_exit.right = invalid_gate.gates.pit_exit.left;
    assert(unconfigured.configure(invalid_gate) ==
           TimingEngineConfigureResult::invalid_gate);
    auto invalid_policy = config;
    invalid_policy.minimum_lap_time_s = 0.0;
    assert(unconfigured.configure(invalid_policy) ==
           TimingEngineConfigureResult::invalid_lap_policy);
    invalid_policy = config;
    invalid_policy.lap_boundary = static_cast<LapBoundary>(255);
    assert(unconfigured.configure(invalid_policy) ==
           TimingEngineConfigureResult::invalid_lap_policy);

    const auto fixes = walking_fixture(projection);
    assert(fixes.size() > track_timer::domain::queue_capacity::gnss_fixes);

    TimingEngine host{};
    TimingEngine target_model{};
    assert(host.configure(config) == TimingEngineConfigureResult::configured);
    assert(target_model.configure(config) == TimingEngineConfigureResult::configured);
    const auto host_replay = replay(host, fixes, false);
    const auto target_replay = replay(target_model, fixes, true);
    assert(host_replay.event_count == 3);
    assert(target_replay.event_count == host_replay.event_count);
    assert(host_replay.gate_record_count == 16);
    assert(target_replay.gate_record_count == host_replay.gate_record_count);
    for (std::size_t index = 0; index < host_replay.event_count; ++index) {
        const auto& expected = host_replay.events[index];
        const auto& actual = target_replay.events[index];
        assert(expected.lap_index == index + 1);
        assert(expected.lap_duration_ns == 8'400'000'000LL);
        assert(actual.crossing_measurement_time_ns ==
               expected.crossing_measurement_time_ns);
        assert(actual.lap_duration_ns == expected.lap_duration_ns);
        assert(actual.segment_sequence_0 == expected.segment_sequence_0);
        assert(actual.segment_sequence_1 == expected.segment_sequence_1);
        assert(actual.quality_flags == expected.quality_flags);
    }
    for (std::size_t index = 0; index < host_replay.gate_record_count; ++index) {
        const auto& expected = host_replay.gate_records[index];
        const auto& actual = target_replay.gate_records[index];
        assert(actual.crossing_measurement_time_ns ==
               expected.crossing_measurement_time_ns);
        assert(actual.intersection_fraction == expected.intersection_fraction);
        assert(actual.segment_sequence_0 == expected.segment_sequence_0);
        assert(actual.segment_sequence_1 == expected.segment_sequence_1);
        assert(actual.gate == expected.gate);
        assert(actual.result == expected.result);
        assert(actual.validation_result == expected.validation_result);
        assert(actual.time_result == expected.time_result);
        assert(actual.has_event == expected.has_event);
    }
    assert(host.lap_snapshot().best_lap_duration_ns == 8'400'000'000LL);
    assert(target_replay.maximum_processing_us < kTimingFixDeadlineUs);

    auto rejected_fix = fixes.front();
    rejected_fix.accepted_for_timing = false;
    assert(target_model.process_fix(rejected_fix, rejected_fix.arrival_monotonic_us)
               .result == TimingEngineResult::source_fix_rejected);

    const std::array configure_results{
        TimingEngineConfigureResult::configured,
        TimingEngineConfigureResult::invalid_reference,
        TimingEngineConfigureResult::invalid_gate,
        TimingEngineConfigureResult::invalid_lap_policy,
    };
    for (const auto result : configure_results) {
        assert(timing_engine_configure_result_name(result)[0] != '\0');
    }
    const std::array engine_results{
        TimingEngineResult::unconfigured,       TimingEngineResult::source_fix_rejected,
        TimingEngineResult::primed,             TimingEngineResult::no_crossing,
        TimingEngineResult::crossing_rejected,  TimingEngineResult::lap_state_updated,
        TimingEngineResult::lap_event,
        TimingEngineResult::gate_events,
    };
    for (const auto result : engine_results) {
        assert(timing_engine_result_name(result)[0] != '\0');
    }

    std::cout << "25 Hz walking replay, exact lap parity, and deadline budget passed\n";
}
