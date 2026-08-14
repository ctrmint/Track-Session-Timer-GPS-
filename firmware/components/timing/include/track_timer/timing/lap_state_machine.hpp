#pragma once

#include "track_timer/domain/contracts.hpp"

#include <cstdint>
#include <type_traits>

namespace track_timer::timing {

enum class LapTimingState : std::uint8_t {
    no_track,
    waiting_for_fix,
    armed,
    lap_running,
    lap_complete,
    rearm_wait,
};

enum class LapStateResult : std::uint8_t {
    no_change,
    track_unavailable,
    waiting_for_fix,
    corridor_exit_required,
    armed,
    lap_started,
    lap_completed,
    rearm_required,
    rearmed,
    below_minimum_lap_time,
    invalid_policy,
    invalid_observation,
    non_monotonic_measurement_time,
};

enum LapSuspectFlag : std::uint32_t {
    lap_suspect_none = 0,
    lap_suspect_below_minimum_time = 1U << 0U,
};

struct LapStatePolicy {
    std::int64_t minimum_lap_time_ns{0};
    double rearm_corridor_m{0.0};
};

// A crossing supplied here has already passed geometry, direction, speed, GNSS
// quality, and crossing-time validation. A non-zero suspect flag records a
// replayable diagnostic without turning the observation into another trigger.
struct LapStateObservation {
    std::int64_t measurement_time_ns{domain::kUnavailableTime};
    std::int64_t crossing_measurement_time_ns{domain::kUnavailableTime};
    double signed_gate_distance_m{0.0};
    double intersection_fraction{0.0};
    std::uint32_t segment_sequence_0{0};
    std::uint32_t segment_sequence_1{0};
    std::uint32_t crossing_suspect_flags{lap_suspect_none};
    bool track_available{false};
    bool timing_fix_valid{false};
    bool valid_crossing{false};
};

struct LapStateSnapshot {
    LapTimingState state{LapTimingState::no_track};
    std::int64_t current_lap_started_ns{domain::kUnavailableTime};
    std::int64_t current_lap_elapsed_ns{domain::kUnavailableTime};
    std::int64_t previous_lap_duration_ns{domain::kUnavailableTime};
    std::int64_t best_lap_duration_ns{domain::kUnavailableTime};
    std::uint32_t lap_index{0};
    std::uint32_t current_lap_suspect_flags{lap_suspect_none};
    std::uint32_t previous_lap_suspect_flags{lap_suspect_none};
};

struct LapStateUpdate {
    LapStateResult result{LapStateResult::no_change};
    LapStateSnapshot snapshot{};
    domain::LapEvent lap_event{};
    bool has_lap_event{false};
};

class LapStateMachine {
public:
    explicit LapStateMachine(LapStatePolicy policy) noexcept;

    [[nodiscard]] LapStateUpdate update(const LapStateObservation& observation) noexcept;
    [[nodiscard]] const LapStateSnapshot& snapshot() const noexcept;
    void reset() noexcept;

private:
    [[nodiscard]] bool valid_policy() const noexcept;
    [[nodiscard]] bool outside_rearm_corridor(double signed_distance_m) const noexcept;
    void update_elapsed(std::int64_t measurement_time_ns) noexcept;
    void clear_runtime() noexcept;

    LapStatePolicy policy_{};
    LapStateSnapshot snapshot_{};
    std::int64_t last_measurement_time_ns_{domain::kUnavailableTime};
};

[[nodiscard]] const char* lap_timing_state_name(LapTimingState state) noexcept;
[[nodiscard]] const char* lap_state_result_name(LapStateResult result) noexcept;

static_assert(std::is_trivially_copyable_v<LapStatePolicy>);
static_assert(std::is_trivially_copyable_v<LapStateObservation>);
static_assert(std::is_trivially_copyable_v<LapStateSnapshot>);
static_assert(std::is_trivially_copyable_v<LapStateUpdate>);

}  // namespace track_timer::timing
