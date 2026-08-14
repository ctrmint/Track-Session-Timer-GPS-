#include "track_timer/timing/lap_state_machine.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

using track_timer::domain::kUnavailableTime;
using track_timer::timing::LapStateMachine;
using track_timer::timing::LapStateObservation;
using track_timer::timing::LapStatePolicy;
using track_timer::timing::LapStateResult;
using track_timer::timing::LapTimingState;
using track_timer::timing::lap_suspect_below_minimum_time;
using track_timer::timing::lap_suspect_none;

constexpr std::int64_t kSecond = 1'000'000'000LL;

LapStateObservation fix(const std::int64_t time_ns, const double distance_m,
                        const bool crossing = false) noexcept
{
    LapStateObservation observation{};
    observation.track_available = true;
    observation.timing_fix_valid = true;
    observation.measurement_time_ns = time_ns;
    observation.signed_gate_distance_m = distance_m;
    observation.valid_crossing = crossing;
    if (crossing) {
        observation.crossing_measurement_time_ns = time_ns;
        observation.intersection_fraction = 0.25;
        observation.segment_sequence_0 = static_cast<std::uint32_t>(time_ns / kSecond);
        observation.segment_sequence_1 = observation.segment_sequence_0 + 1;
    }
    return observation;
}

void start_lap(LapStateMachine& machine, const std::int64_t crossing_time_ns)
{
    assert(machine.update(fix(crossing_time_ns - 2 * kSecond, 11.0)).result ==
           LapStateResult::armed);
    const auto started = machine.update(fix(crossing_time_ns, -1.0, true));
    assert(started.result == LapStateResult::lap_started);
    assert(started.snapshot.state == LapTimingState::rearm_wait);
    assert(started.snapshot.lap_index == 1);
    assert(!started.has_lap_event);
    assert(machine.update(fix(crossing_time_ns + kSecond, -11.0)).result ==
           LapStateResult::rearmed);
    assert(machine.snapshot().state == LapTimingState::lap_running);
}

}  // namespace

int main()
{
    LapStateMachine invalid_policy({0, 10.0});
    assert(invalid_policy.update({}).result == LapStateResult::invalid_policy);

    LapStateMachine startup({30 * kSecond, 10.0});
    auto update = startup.update({});
    assert(update.result == LapStateResult::track_unavailable);
    assert(update.snapshot.state == LapTimingState::no_track);

    LapStateObservation no_fix{};
    no_fix.track_available = true;
    update = startup.update(no_fix);
    assert(update.result == LapStateResult::waiting_for_fix);
    assert(update.snapshot.state == LapTimingState::waiting_for_fix);

    update = startup.update(fix(1 * kSecond, 0.0, true));
    assert(update.result == LapStateResult::corridor_exit_required);
    assert(update.snapshot.lap_index == 0);
    assert(!update.has_lap_event);
    update = startup.update(fix(2 * kSecond, 10.0));
    assert(update.result == LapStateResult::corridor_exit_required);
    update = startup.update(fix(3 * kSecond, 10.001));
    assert(update.result == LapStateResult::armed);

    update = startup.update(fix(4 * kSecond, -1.0, true));
    assert(update.result == LapStateResult::lap_started);
    update = startup.update(fix(5 * kSecond, 1.0, true));
    assert(update.result == LapStateResult::rearm_required);
    assert(!update.has_lap_event);
    update = startup.update(fix(6 * kSecond, -11.0));
    assert(update.result == LapStateResult::rearmed);

    update = startup.update(fix(20 * kSecond, 1.0, true));
    assert(update.result == LapStateResult::below_minimum_lap_time);
    assert(update.snapshot.state == LapTimingState::rearm_wait);
    assert((update.snapshot.current_lap_suspect_flags &
            lap_suspect_below_minimum_time) != 0U);
    assert(!update.has_lap_event);
    assert(startup.update(fix(21 * kSecond, 1.0, true)).result ==
           LapStateResult::rearm_required);
    assert(startup.update(fix(22 * kSecond, 11.0)).result == LapStateResult::rearmed);
    update = startup.update(fix(40 * kSecond, -1.0, true));
    assert(update.result == LapStateResult::lap_completed);
    assert(update.has_lap_event);
    assert(update.lap_event.lap_index == 1);
    assert(update.lap_event.lap_duration_ns == 36 * kSecond);
    assert((update.lap_event.quality_flags & lap_suspect_below_minimum_time) != 0U);
    assert(update.snapshot.previous_lap_duration_ns == 36 * kSecond);
    assert(update.snapshot.best_lap_duration_ns == kUnavailableTime);
    assert(update.snapshot.lap_index == 2);
    assert(update.snapshot.state == LapTimingState::lap_complete);
    assert(startup.update(fix(41 * kSecond, -1.0)).result ==
           LapStateResult::rearm_required);
    assert(startup.update(fix(42 * kSecond, -11.0)).result == LapStateResult::rearmed);

    const auto before_bad_time = startup.snapshot();
    update = startup.update(fix(41 * kSecond, -12.0));
    assert(update.result == LapStateResult::non_monotonic_measurement_time);
    assert(update.snapshot.state == before_bad_time.state);
    assert(update.snapshot.lap_index == before_bad_time.lap_index);

    LapStateMachine laps({20 * kSecond, 10.0});
    start_lap(laps, 10 * kSecond);
    update = laps.update(fix(70 * kSecond, 0.0, true));
    assert(update.result == LapStateResult::lap_completed);
    assert(update.lap_event.lap_duration_ns == 60 * kSecond);
    assert(update.snapshot.best_lap_duration_ns == 60 * kSecond);
    assert(update.snapshot.previous_lap_suspect_flags == lap_suspect_none);
    assert(laps.update(fix(71 * kSecond, 0.0)).result ==
           LapStateResult::rearm_required);
    assert(laps.update(fix(72 * kSecond, 11.0)).result == LapStateResult::rearmed);
    update = laps.update(fix(125 * kSecond, 0.0, true));
    assert(update.has_lap_event);
    assert(update.lap_event.lap_index == 2);
    assert(update.lap_event.lap_duration_ns == 55 * kSecond);
    assert(update.snapshot.previous_lap_duration_ns == 55 * kSecond);
    assert(update.snapshot.best_lap_duration_ns == 55 * kSecond);
    assert(update.snapshot.current_lap_elapsed_ns == 0);

    assert(laps.update({}).snapshot.state == LapTimingState::no_track);
    assert(laps.snapshot().lap_index == 0);
    assert(laps.snapshot().best_lap_duration_ns == kUnavailableTime);

    const std::array states{
        LapTimingState::no_track,       LapTimingState::waiting_for_fix,
        LapTimingState::armed,          LapTimingState::lap_running,
        LapTimingState::lap_complete,   LapTimingState::rearm_wait,
    };
    for (const auto state : states) {
        assert(track_timer::timing::lap_timing_state_name(state)[0] != '\0');
    }
    const std::array results{
        LapStateResult::no_change,
        LapStateResult::track_unavailable,
        LapStateResult::waiting_for_fix,
        LapStateResult::corridor_exit_required,
        LapStateResult::armed,
        LapStateResult::lap_started,
        LapStateResult::lap_completed,
        LapStateResult::rearm_required,
        LapStateResult::rearmed,
        LapStateResult::below_minimum_lap_time,
        LapStateResult::invalid_policy,
        LapStateResult::invalid_observation,
        LapStateResult::non_monotonic_measurement_time,
    };
    for (const auto result : results) {
        assert(track_timer::timing::lap_state_result_name(result)[0] != '\0');
    }

    std::cout << "Deterministic lap lifecycle, rearm, and minimum-time rules passed\n";
}
