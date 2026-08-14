#include "track_timer/timing/lap_state_machine.hpp"

#include <cmath>
#include <limits>

namespace track_timer::timing {
namespace {

[[nodiscard]] bool valid_time(const std::int64_t time_ns) noexcept
{
    return time_ns >= 0;
}

}  // namespace

LapStateMachine::LapStateMachine(const LapStatePolicy policy) noexcept : policy_(policy) {}

LapStateUpdate LapStateMachine::update(const LapStateObservation& observation) noexcept
{
    LapStateUpdate result{};
    if (!valid_policy()) {
        result.result = LapStateResult::invalid_policy;
        result.snapshot = snapshot_;
        return result;
    }

    if (!observation.track_available) {
        clear_runtime();
        result.result = LapStateResult::track_unavailable;
        result.snapshot = snapshot_;
        return result;
    }

    if (snapshot_.state == LapTimingState::no_track) {
        snapshot_.state = LapTimingState::waiting_for_fix;
    }
    if (!observation.timing_fix_valid) {
        result.result = snapshot_.lap_index == 0 ? LapStateResult::waiting_for_fix
                                                : LapStateResult::no_change;
        result.snapshot = snapshot_;
        return result;
    }
    if (!valid_time(observation.measurement_time_ns) ||
        !std::isfinite(observation.signed_gate_distance_m) ||
        (observation.valid_crossing &&
         (!valid_time(observation.crossing_measurement_time_ns) ||
          !std::isfinite(observation.intersection_fraction) ||
          observation.intersection_fraction < 0.0 ||
          observation.intersection_fraction > 1.0))) {
        result.result = LapStateResult::invalid_observation;
        result.snapshot = snapshot_;
        return result;
    }
    if (last_measurement_time_ns_ != domain::kUnavailableTime &&
        observation.measurement_time_ns < last_measurement_time_ns_) {
        result.result = LapStateResult::non_monotonic_measurement_time;
        result.snapshot = snapshot_;
        return result;
    }

    last_measurement_time_ns_ = observation.measurement_time_ns;
    update_elapsed(observation.measurement_time_ns);
    const auto outside = outside_rearm_corridor(observation.signed_gate_distance_m);

    switch (snapshot_.state) {
    case LapTimingState::no_track:
        break;
    case LapTimingState::waiting_for_fix:
        if (outside) {
            snapshot_.state = LapTimingState::armed;
            result.result = LapStateResult::armed;
        } else {
            result.result = LapStateResult::corridor_exit_required;
        }
        break;
    case LapTimingState::armed:
        if (observation.valid_crossing) {
            snapshot_.current_lap_started_ns = observation.crossing_measurement_time_ns;
            snapshot_.current_lap_elapsed_ns = 0;
            snapshot_.current_lap_suspect_flags = observation.crossing_suspect_flags;
            snapshot_.lap_index = 1;
            snapshot_.state = LapTimingState::rearm_wait;
            result.result = LapStateResult::lap_started;
        }
        break;
    case LapTimingState::lap_running:
        if (observation.valid_crossing) {
            const auto crossing_time = observation.crossing_measurement_time_ns;
            if (crossing_time <= snapshot_.current_lap_started_ns) {
                result.result = LapStateResult::invalid_observation;
                break;
            }
            const auto duration = crossing_time - snapshot_.current_lap_started_ns;
            if (duration < policy_.minimum_lap_time_ns) {
                snapshot_.current_lap_suspect_flags |=
                    lap_suspect_below_minimum_time;
                snapshot_.state = LapTimingState::rearm_wait;
                result.result = LapStateResult::below_minimum_lap_time;
                break;
            }

            const auto suspect_flags = snapshot_.current_lap_suspect_flags |
                                       observation.crossing_suspect_flags;
            result.lap_event = {
                snapshot_.lap_index,
                crossing_time,
                duration,
                observation.intersection_fraction,
                observation.segment_sequence_0,
                observation.segment_sequence_1,
                suspect_flags,
            };
            result.has_lap_event = true;
            snapshot_.previous_lap_duration_ns = duration;
            snapshot_.previous_lap_suspect_flags = suspect_flags;
            if (suspect_flags == lap_suspect_none &&
                (snapshot_.best_lap_duration_ns == domain::kUnavailableTime ||
                 duration < snapshot_.best_lap_duration_ns)) {
                snapshot_.best_lap_duration_ns = duration;
            }
            ++snapshot_.lap_index;
            snapshot_.current_lap_started_ns = crossing_time;
            snapshot_.current_lap_elapsed_ns = 0;
            snapshot_.current_lap_suspect_flags = observation.crossing_suspect_flags;
            snapshot_.state = LapTimingState::lap_complete;
            result.result = LapStateResult::lap_completed;
        }
        break;
    case LapTimingState::lap_complete:
        snapshot_.state = outside ? LapTimingState::lap_running
                                  : LapTimingState::rearm_wait;
        result.result = outside ? LapStateResult::rearmed
                                : LapStateResult::rearm_required;
        break;
    case LapTimingState::rearm_wait:
        if (outside) {
            snapshot_.state = snapshot_.lap_index == 0 ? LapTimingState::armed
                                                       : LapTimingState::lap_running;
            result.result = LapStateResult::rearmed;
        } else if (observation.valid_crossing) {
            result.result = LapStateResult::rearm_required;
        }
        break;
    }

    result.snapshot = snapshot_;
    return result;
}

const LapStateSnapshot& LapStateMachine::snapshot() const noexcept { return snapshot_; }

void LapStateMachine::reset() noexcept { clear_runtime(); }

bool LapStateMachine::valid_policy() const noexcept
{
    return policy_.minimum_lap_time_ns > 0 &&
           policy_.minimum_lap_time_ns < std::numeric_limits<std::int64_t>::max() &&
           std::isfinite(policy_.rearm_corridor_m) && policy_.rearm_corridor_m > 0.0;
}

bool LapStateMachine::outside_rearm_corridor(const double signed_distance_m) const noexcept
{
    return std::abs(signed_distance_m) > policy_.rearm_corridor_m;
}

void LapStateMachine::update_elapsed(const std::int64_t measurement_time_ns) noexcept
{
    if (snapshot_.current_lap_started_ns != domain::kUnavailableTime &&
        measurement_time_ns >= snapshot_.current_lap_started_ns) {
        snapshot_.current_lap_elapsed_ns =
            measurement_time_ns - snapshot_.current_lap_started_ns;
    }
}

void LapStateMachine::clear_runtime() noexcept
{
    snapshot_ = {};
    last_measurement_time_ns_ = domain::kUnavailableTime;
}

const char* lap_timing_state_name(const LapTimingState state) noexcept
{
    switch (state) {
    case LapTimingState::no_track:
        return "no-track";
    case LapTimingState::waiting_for_fix:
        return "waiting-for-fix";
    case LapTimingState::armed:
        return "armed";
    case LapTimingState::lap_running:
        return "lap-running";
    case LapTimingState::lap_complete:
        return "lap-complete";
    case LapTimingState::rearm_wait:
        return "rearm-wait";
    }
    return "unknown";
}

const char* lap_state_result_name(const LapStateResult result) noexcept
{
    switch (result) {
    case LapStateResult::no_change:
        return "no-change";
    case LapStateResult::track_unavailable:
        return "track-unavailable";
    case LapStateResult::waiting_for_fix:
        return "waiting-for-fix";
    case LapStateResult::corridor_exit_required:
        return "corridor-exit-required";
    case LapStateResult::armed:
        return "armed";
    case LapStateResult::lap_started:
        return "lap-started";
    case LapStateResult::lap_completed:
        return "lap-completed";
    case LapStateResult::rearm_required:
        return "rearm-required";
    case LapStateResult::rearmed:
        return "rearmed";
    case LapStateResult::below_minimum_lap_time:
        return "below-minimum-lap-time";
    case LapStateResult::invalid_policy:
        return "invalid-policy";
    case LapStateResult::invalid_observation:
        return "invalid-observation";
    case LapStateResult::non_monotonic_measurement_time:
        return "non-monotonic-measurement-time";
    }
    return "unknown";
}

}  // namespace track_timer::timing
