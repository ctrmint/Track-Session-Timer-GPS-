#include "track_timer/timing/engine.hpp"

#include <cmath>
#include <limits>

namespace track_timer::timing {
namespace {

constexpr double kNanosecondsPerSecond = 1'000'000'000.0;

[[nodiscard]] bool valid_gate(const track::DirectedGateDefinition& gate) noexcept
{
    return std::isfinite(gate.direction_heading_deg) &&
           gate.direction_heading_deg >= 0.0 && gate.direction_heading_deg < 360.0 &&
           std::isfinite(gate.heading_tolerance_deg) &&
           gate.heading_tolerance_deg >= 0.0 && gate.heading_tolerance_deg <= 180.0 &&
           std::isfinite(gate.minimum_crossing_speed_mps) &&
           gate.minimum_crossing_speed_mps > 0.0 &&
           std::isfinite(gate.rearm_corridor_m) && gate.rearm_corridor_m > 0.0;
}

[[nodiscard]] bool valid_lap_time(const double seconds) noexcept
{
    return std::isfinite(seconds) && seconds > 0.0 &&
           seconds <= static_cast<double>(std::numeric_limits<std::int64_t>::max()) /
                          kNanosecondsPerSecond;
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
    if (!valid_gate(config.lap_gate) ||
        track::project_to_circuit_local(projection_, config.lap_gate.left,
                                        gate_.local_left) !=
            track::ProjectionResult::projected ||
        track::project_to_circuit_local(projection_, config.lap_gate.right,
                                        gate_.local_right) !=
            track::ProjectionResult::projected ||
        std::hypot(gate_.local_right.east_m - gate_.local_left.east_m,
                   gate_.local_right.north_m - gate_.local_left.north_m) <=
            kMinimumIntersectionSegmentLengthM) {
        reset();
        return TimingEngineConfigureResult::invalid_gate;
    }
    if (!valid_lap_time(config.minimum_lap_time_s)) {
        reset();
        return TimingEngineConfigureResult::invalid_lap_policy;
    }

    config_ = config;
    gate_.left = config.lap_gate.left;
    gate_.right = config.lap_gate.right;
    gate_.direction_heading_deg = config.lap_gate.direction_heading_deg;
    gate_.heading_tolerance_deg = config.lap_gate.heading_tolerance_deg;
    gate_.minimum_crossing_speed_mps = config.lap_gate.minimum_crossing_speed_mps;
    gate_.rearm_corridor_m = config.lap_gate.rearm_corridor_m;
    const auto minimum_lap_time_ns = static_cast<std::int64_t>(
        std::llround(config.minimum_lap_time_s * kNanosecondsPerSecond));
    lap_state_machine_ =
        LapStateMachine({minimum_lap_time_ns, gate_.rearm_corridor_m});
    configured_ = true;
    return TimingEngineConfigureResult::configured;
}

TimingEngineDecision TimingEngine::process_fix(
    const domain::GnssFix& fix, const std::int64_t evaluation_monotonic_us) noexcept
{
    TimingEngineDecision decision{};
    decision.fix_sequence = fix.sequence_number;
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
    observation.signed_gate_distance_m = signed_gate_distance_m(current_position);
    if (!has_previous_fix_) {
        decision.lap_update = lap_state_machine_.update(observation);
        previous_fix_ = fix;
        previous_position_ = current_position;
        has_previous_fix_ = true;
        decision.result = TimingEngineResult::primed;
        return decision;
    }

    decision.intersection = intersect_movement_with_gate(
        previous_position_, current_position, gate_.local_left, gate_.local_right);
    if (decision.intersection.result == SegmentIntersectionResult::intersection) {
        decision.crossing_validation = validate_crossing(
            previous_fix_, previous_position_, fix, current_position, gate_,
            decision.intersection, config_.quality_thresholds);
        if (decision.crossing_validation.result == CrossingValidationResult::accepted) {
            decision.crossing_time = interpolate_crossing_measurement_time(
                previous_fix_, fix, decision.intersection.movement_fraction,
                evaluation_monotonic_us, config_.time_policy);
            if (decision.crossing_time.result == CrossingTimeResult::interpolated) {
                observation.valid_crossing = true;
                observation.crossing_measurement_time_ns =
                    decision.crossing_time.crossing_measurement_time_ns;
                observation.intersection_fraction =
                    decision.intersection.movement_fraction;
                observation.segment_sequence_0 = previous_fix_.sequence_number;
                observation.segment_sequence_1 = fix.sequence_number;
            }
        }
    }

    decision.lap_update = lap_state_machine_.update(observation);
    previous_fix_ = fix;
    previous_position_ = current_position;
    if (decision.lap_update.has_lap_event) {
        decision.result = TimingEngineResult::lap_event;
    } else if (observation.valid_crossing) {
        decision.result = TimingEngineResult::lap_state_updated;
    } else if (decision.intersection.result == SegmentIntersectionResult::intersection) {
        decision.result = TimingEngineResult::crossing_rejected;
    } else {
        decision.result = TimingEngineResult::no_crossing;
    }
    return decision;
}

void TimingEngine::reset() noexcept
{
    config_ = {};
    projection_ = {};
    gate_ = {};
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

double TimingEngine::signed_gate_distance_m(const track::LocalPoint& point) const noexcept
{
    const auto gate_east_m = gate_.local_right.east_m - gate_.local_left.east_m;
    const auto gate_north_m = gate_.local_right.north_m - gate_.local_left.north_m;
    const auto gate_length_m = std::hypot(gate_east_m, gate_north_m);
    return (gate_east_m * (point.north_m - gate_.local_left.north_m) -
            gate_north_m * (point.east_m - gate_.local_left.east_m)) /
           gate_length_m;
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
    }
    return "unknown";
}

}  // namespace track_timer::timing
