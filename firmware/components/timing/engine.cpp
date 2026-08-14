#include "track_timer/timing/engine.hpp"

#include <cmath>
#include <limits>

namespace track_timer::timing {
namespace {

constexpr double kNanosecondsPerSecond = 1'000'000'000.0;

constexpr std::array<TimingGate, kTimingGateCount> kGateTypes{
    TimingGate::start,
    TimingGate::finish,
    TimingGate::pit_entry,
    TimingGate::pit_exit,
};

std::array<const track::DirectedGateDefinition*, kTimingGateCount> config_gates(
    const TimingEngineConfig& config) noexcept
{
    return {&config.gates.start, &config.gates.finish, &config.gates.pit_entry,
            &config.gates.pit_exit};
}

[[nodiscard]] bool valid_gate(const track::DirectedGateDefinition& gate) noexcept
{
    return std::isfinite(gate.direction_heading_deg) &&
           gate.direction_heading_deg >= 0.0 && gate.direction_heading_deg < 360.0 &&
           std::isfinite(gate.heading_tolerance_deg) &&
           gate.heading_tolerance_deg >= 0.0 && gate.heading_tolerance_deg <= 180.0 &&
           std::isfinite(gate.minimum_crossing_speed_mps) &&
           gate.minimum_crossing_speed_mps > 0.0 &&
           gate.minimum_crossing_speed_mps <= 150.0 &&
           std::isfinite(gate.rearm_corridor_m) && gate.rearm_corridor_m > 0.0 &&
           gate.rearm_corridor_m <= 1'000.0;
}

[[nodiscard]] bool valid_lap_time(const double seconds) noexcept
{
    return std::isfinite(seconds) && seconds > 0.0 &&
           seconds <= static_cast<double>(std::numeric_limits<std::int64_t>::max()) /
                          kNanosecondsPerSecond;
}

[[nodiscard]] bool valid_lap_boundary(const LapBoundary boundary) noexcept
{
    return boundary == LapBoundary::start || boundary == LapBoundary::finish;
}

}  // namespace

TimingEngine::TimingEngine() noexcept = default;

TimingEngineConfigureResult TimingEngine::configure(
    const TimingEngineConfig& config) noexcept
{
    reset();
    if (track::configure_circuit_projection(config.reference, projection_) !=
        track::ProjectionResult::projected) {
        return TimingEngineConfigureResult::invalid_reference;
    }
    const auto source_gates = config_gates(config);
    for (std::size_t index = 0; index < gates_.size(); ++index) {
        const auto& source = *source_gates[index];
        auto& projected = gates_[index];
        if (!valid_gate(source) ||
            track::project_to_circuit_local(projection_, source.left,
                                            projected.local_left) !=
                track::ProjectionResult::projected ||
            track::project_to_circuit_local(projection_, source.right,
                                            projected.local_right) !=
                track::ProjectionResult::projected ||
            std::hypot(projected.local_right.east_m - projected.local_left.east_m,
                       projected.local_right.north_m - projected.local_left.north_m) <=
                kMinimumIntersectionSegmentLengthM) {
            reset();
            return TimingEngineConfigureResult::invalid_gate;
        }
        projected.left = source.left;
        projected.right = source.right;
        projected.direction_heading_deg = source.direction_heading_deg;
        projected.heading_tolerance_deg = source.heading_tolerance_deg;
        projected.minimum_crossing_speed_mps = source.minimum_crossing_speed_mps;
        projected.rearm_corridor_m = source.rearm_corridor_m;
    }
    if (!valid_lap_time(config.minimum_lap_time_s) ||
        !valid_lap_boundary(config.lap_boundary)) {
        reset();
        return TimingEngineConfigureResult::invalid_lap_policy;
    }

    config_ = config;
    const auto minimum_lap_time_ns = static_cast<std::int64_t>(
        std::llround(config.minimum_lap_time_s * kNanosecondsPerSecond));
    const auto lap_gate_index = config.lap_boundary == LapBoundary::start ? 0U : 1U;
    lap_state_machine_ = LapStateMachine(
        {minimum_lap_time_ns, gates_[lap_gate_index].rearm_corridor_m});
    configured_ = true;
    return TimingEngineConfigureResult::configured;
}

TimingEngineDecision TimingEngine::process_fix(
    const domain::GnssFix& fix, const std::int64_t evaluation_monotonic_us) noexcept
{
    TimingEngineDecision decision{};
    decision.fix_sequence = fix.sequence_number;
    for (std::size_t index = 0; index < decision.gate_decisions.size(); ++index) {
        decision.gate_decisions[index].gate = kGateTypes[index];
    }
    if (!configured_) {
        return decision;
    }

    LapStateObservation observation{};
    observation.track_available = true;
    observation.measurement_time_ns = fix.measurement_time_ns;
    track::LocalPoint current_position{};
    decision.projection_result = track::project_to_circuit_local(
        projection_, {fix.latitude_deg, fix.longitude_deg}, current_position);
    if (!fix.accepted_for_timing || fix.reject_reason != domain::FixRejectReason::none ||
        decision.projection_result != track::ProjectionResult::projected) {
        decision.lap_update = lap_state_machine_.update(observation);
        decision.result = TimingEngineResult::source_fix_rejected;
        has_previous_fix_ = false;
        return decision;
    }

    observation.timing_fix_valid = true;
    const auto lap_gate_index = selected_lap_gate_index();
    observation.signed_gate_distance_m =
        signed_gate_distance_m(gates_[lap_gate_index], current_position);
    if (!has_previous_fix_) {
        decision.lap_update = lap_state_machine_.update(observation);
        previous_fix_ = fix;
        previous_position_ = current_position;
        has_previous_fix_ = true;
        decision.result = TimingEngineResult::primed;
        return decision;
    }

    bool has_gate_event = false;
    bool has_rejected_crossing = false;
    for (std::size_t index = 0; index < gates_.size(); ++index) {
        auto& gate_decision = decision.gate_decisions[index];
        const auto& gate = gates_[index];
        if (!gate_armed_[index] &&
            std::abs(signed_gate_distance_m(gate, current_position)) >
                gate.rearm_corridor_m) {
            gate_armed_[index] = true;
        }

        const auto intersection = intersect_movement_with_gate(
            previous_position_, current_position, gate.local_left, gate.local_right);
        gate_decision.intersection_result = intersection.result;
        gate_decision.intersection_fraction = intersection.movement_fraction;
        gate_decision.segment_sequence_0 = previous_fix_.sequence_number;
        gate_decision.segment_sequence_1 = fix.sequence_number;
        if (intersection.result != SegmentIntersectionResult::intersection) {
            gate_decision.result = GateCrossingResult::no_intersection;
            continue;
        }

        const auto validation = validate_crossing(
            previous_fix_, previous_position_, fix, current_position, gate,
            intersection, config_.quality_thresholds);
        gate_decision.validation_result = validation.result;
        gate_decision.crossing_speed_mps = validation.crossing_speed_mps;
        if (validation.result != CrossingValidationResult::accepted) {
            gate_decision.result = GateCrossingResult::crossing_rejected;
            has_rejected_crossing = true;
            continue;
        }

        const auto crossing_time = interpolate_crossing_measurement_time(
            previous_fix_, fix, intersection.movement_fraction,
            evaluation_monotonic_us, config_.time_policy);
        gate_decision.time_result = crossing_time.result;
        gate_decision.crossing_measurement_time_ns =
            crossing_time.crossing_measurement_time_ns;
        if (crossing_time.result != CrossingTimeResult::interpolated) {
            gate_decision.result = GateCrossingResult::timestamp_rejected;
            has_rejected_crossing = true;
            continue;
        }
        if (!gate_armed_[index]) {
            gate_decision.result = GateCrossingResult::rearm_required;
            has_rejected_crossing = true;
            continue;
        }

        gate_decision.result = GateCrossingResult::event;
        gate_decision.has_event = true;
        gate_armed_[index] = false;
        has_gate_event = true;
        if (index == lap_gate_index) {
            observation.valid_crossing = true;
            observation.crossing_measurement_time_ns =
                crossing_time.crossing_measurement_time_ns;
            observation.intersection_fraction = intersection.movement_fraction;
            observation.segment_sequence_0 = previous_fix_.sequence_number;
            observation.segment_sequence_1 = fix.sequence_number;
        }
    }

    decision.lap_update = lap_state_machine_.update(observation);
    previous_fix_ = fix;
    previous_position_ = current_position;
    if (decision.lap_update.has_lap_event) {
        decision.result = TimingEngineResult::lap_event;
    }
    else if (observation.valid_crossing) {
        decision.result = TimingEngineResult::lap_state_updated;
    }
    else if (has_gate_event) {
        decision.result = TimingEngineResult::gate_events;
    }
    else if (has_rejected_crossing) {
        decision.result = TimingEngineResult::crossing_rejected;
    }
    else {
        decision.result = TimingEngineResult::no_crossing;
    }
    return decision;
}

void TimingEngine::reset() noexcept
{
    config_ = {};
    projection_ = {};
    gates_ = {};
    gate_armed_ = {{true, true, true, true}};
    lap_state_machine_.reset();
    previous_fix_ = {};
    previous_position_ = {};
    configured_ = false;
    has_previous_fix_ = false;
}

bool TimingEngine::configured() const noexcept { return configured_; }

const LapStateSnapshot& TimingEngine::lap_snapshot() const noexcept
{
    return lap_state_machine_.snapshot();
}

double TimingEngine::signed_gate_distance_m(
    const track::DirectedGateDefinition& gate,
    const track::LocalPoint& point) const noexcept
{
    const auto gate_east_m = gate.local_right.east_m - gate.local_left.east_m;
    const auto gate_north_m = gate.local_right.north_m - gate.local_left.north_m;
    const auto gate_length_m = std::hypot(gate_east_m, gate_north_m);
    return (gate_east_m * (point.north_m - gate.local_left.north_m) -
            gate_north_m * (point.east_m - gate.local_left.east_m)) /
           gate_length_m;
}

std::size_t TimingEngine::selected_lap_gate_index() const noexcept
{
    return config_.lap_boundary == LapBoundary::start ? 0U : 1U;
}

const char* timing_engine_configure_result_name(
    const TimingEngineConfigureResult result) noexcept
{
    switch (result) {
    case TimingEngineConfigureResult::configured:
        return "configured";
    case TimingEngineConfigureResult::invalid_reference:
        return "invalid-reference";
    case TimingEngineConfigureResult::invalid_gate:
        return "invalid-gate";
    case TimingEngineConfigureResult::invalid_lap_policy:
        return "invalid-lap-policy";
    }
    return "unknown";
}

const char* timing_engine_result_name(const TimingEngineResult result) noexcept
{
    switch (result) {
    case TimingEngineResult::unconfigured:
        return "unconfigured";
    case TimingEngineResult::source_fix_rejected:
        return "source-fix-rejected";
    case TimingEngineResult::primed:
        return "primed";
    case TimingEngineResult::no_crossing:
        return "no-crossing";
    case TimingEngineResult::crossing_rejected:
        return "crossing-rejected";
    case TimingEngineResult::lap_state_updated:
        return "lap-state-updated";
    case TimingEngineResult::lap_event:
        return "lap-event";
    case TimingEngineResult::gate_events:
        return "gate-events";
    }
    return "unknown";
}

const char* timing_gate_name(const TimingGate gate) noexcept
{
    switch (gate) {
    case TimingGate::start:
        return "start";
    case TimingGate::finish:
        return "finish";
    case TimingGate::pit_entry:
        return "pit-entry";
    case TimingGate::pit_exit:
        return "pit-exit";
    }
    return "unknown";
}

const char* gate_crossing_result_name(const GateCrossingResult result) noexcept
{
    switch (result) {
    case GateCrossingResult::not_evaluated:
        return "not-evaluated";
    case GateCrossingResult::no_intersection:
        return "no-intersection";
    case GateCrossingResult::crossing_rejected:
        return "crossing-rejected";
    case GateCrossingResult::timestamp_rejected:
        return "timestamp-rejected";
    case GateCrossingResult::rearm_required:
        return "rearm-required";
    case GateCrossingResult::event:
        return "event";
    }
    return "unknown";
}

}  // namespace track_timer::timing
