#pragma once

#include "track_timer/timing/crossing_time.hpp"
#include "track_timer/timing/crossing_validation.hpp"
#include "track_timer/timing/lap_state_machine.hpp"
#include "track_timer/track/projection.hpp"

#include <cstdint>
#include <type_traits>

namespace track_timer::timing {

enum class TimingEngineConfigureResult : std::uint8_t {
    configured,
    invalid_reference,
    invalid_gate,
    invalid_lap_policy,
};

struct TimingEngineConfig {
    track::GeographicPoint reference{};
    track::DirectedGateDefinition lap_gate{};
    double minimum_lap_time_s{0.0};
    CrossingQualityThresholds quality_thresholds{};
    CrossingTimePolicy time_policy{};
};

enum class TimingEngineResult : std::uint8_t {
    unconfigured,
    source_fix_rejected,
    primed,
    no_crossing,
    crossing_rejected,
    lap_state_updated,
    lap_event,
};

struct TimingEngineDecision {
    TimingEngineResult result{TimingEngineResult::unconfigured};
    track::ProjectionResult projection_result{track::ProjectionResult::unconfigured};
    SegmentIntersection intersection{};
    CrossingValidationDecision crossing_validation{};
    CrossingTimeDecision crossing_time{};
    LapStateUpdate lap_update{};
    std::uint32_t fix_sequence{0};
};

class TimingEngine {
public:
    TimingEngine() noexcept;

    [[nodiscard]] TimingEngineConfigureResult configure(
        const TimingEngineConfig& config) noexcept;
    [[nodiscard]] TimingEngineDecision process_fix(
        const domain::GnssFix& fix, std::int64_t evaluation_monotonic_us) noexcept;
    void reset() noexcept;

    [[nodiscard]] bool configured() const noexcept;
    [[nodiscard]] const LapStateSnapshot& lap_snapshot() const noexcept;

private:
    [[nodiscard]] double signed_gate_distance_m(
        const track::LocalPoint& point) const noexcept;

    TimingEngineConfig config_{};
    track::CircuitProjection projection_{};
    track::DirectedGateDefinition gate_{};
    LapStateMachine lap_state_machine_{{1, 1.0}};
    domain::GnssFix previous_fix_{};
    track::LocalPoint previous_position_{};
    bool configured_{false};
    bool has_previous_fix_{false};
};

[[nodiscard]] const char* timing_engine_configure_result_name(
    TimingEngineConfigureResult result) noexcept;
[[nodiscard]] const char* timing_engine_result_name(TimingEngineResult result) noexcept;

static_assert(std::is_trivially_copyable_v<TimingEngineConfig>);
static_assert(std::is_trivially_copyable_v<TimingEngineDecision>);

}  // namespace track_timer::timing
