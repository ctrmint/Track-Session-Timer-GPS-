#pragma once

#include "track_timer/timing/crossing_time.hpp"
#include "track_timer/timing/crossing_validation.hpp"
#include "track_timer/timing/lap_state_machine.hpp"
#include "track_timer/track/projection.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::timing {

enum class TimingEngineConfigureResult : std::uint8_t {
    configured,
    invalid_reference,
    invalid_gate,
    invalid_lap_policy,
};

enum class LapBoundary : std::uint8_t {
    start,
    finish,
};

enum class TimingGate : std::uint8_t {
    start,
    finish,
    pit_entry,
    pit_exit,
};

inline constexpr std::size_t kTimingGateCount = 4;

struct TimingEngineConfig {
    track::GeographicPoint reference{};
    track::CircuitGateDefinitions gates{};
    LapBoundary lap_boundary{LapBoundary::finish};
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
    gate_events,
};

enum class GateCrossingResult : std::uint8_t {
    not_evaluated,
    no_intersection,
    crossing_rejected,
    timestamp_rejected,
    rearm_required,
    event,
};

struct GateCrossingDecision {
    std::int64_t crossing_measurement_time_ns{domain::kUnavailableTime};
    double intersection_fraction{0.0};
    float crossing_speed_mps{0.0F};
    std::uint32_t segment_sequence_0{0};
    std::uint32_t segment_sequence_1{0};
    TimingGate gate{TimingGate::start};
    GateCrossingResult result{GateCrossingResult::not_evaluated};
    SegmentIntersectionResult intersection_result{
        SegmentIntersectionResult::no_intersection};
    CrossingValidationResult validation_result{
        CrossingValidationResult::invalid_configuration};
    CrossingTimeResult time_result{CrossingTimeResult::invalid_policy};
    bool has_event{false};
};

struct TimingEngineDecision {
    TimingEngineResult result{TimingEngineResult::unconfigured};
    track::ProjectionResult projection_result{track::ProjectionResult::unconfigured};
    std::array<GateCrossingDecision, kTimingGateCount> gate_decisions{};
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
        const track::DirectedGateDefinition& gate,
        const track::LocalPoint& point) const noexcept;
    [[nodiscard]] std::size_t selected_lap_gate_index() const noexcept;

    TimingEngineConfig config_{};
    track::CircuitProjection projection_{};
    std::array<track::DirectedGateDefinition, kTimingGateCount> gates_{};
    std::array<bool, kTimingGateCount> gate_armed_{{true, true, true, true}};
    LapStateMachine lap_state_machine_{{1, 1.0}};
    domain::GnssFix previous_fix_{};
    track::LocalPoint previous_position_{};
    bool configured_{false};
    bool has_previous_fix_{false};
};

[[nodiscard]] const char* timing_engine_configure_result_name(
    TimingEngineConfigureResult result) noexcept;
[[nodiscard]] const char* timing_engine_result_name(TimingEngineResult result) noexcept;
[[nodiscard]] const char* timing_gate_name(TimingGate gate) noexcept;
[[nodiscard]] const char* gate_crossing_result_name(GateCrossingResult result) noexcept;

static_assert(std::is_trivially_copyable_v<TimingEngineConfig>);
static_assert(std::is_trivially_copyable_v<TimingEngineDecision>);
static_assert(std::is_trivially_copyable_v<GateCrossingDecision>);
static_assert(sizeof(GateCrossingDecision) <= 48);

}  // namespace track_timer::timing
