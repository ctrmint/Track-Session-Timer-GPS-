#pragma once

#include "track_timer/timing/engine.hpp"

#include <cstddef>
#include <cstdint>

namespace track_timer::timing {

inline constexpr std::int64_t kTimingFixDeadlineUs = 40'000;

// What happened when a new circuit was handed to a runtime that is already timing.
enum class TimingReconfigureResult : std::uint8_t {
    // Nothing to reconfigure. The caller wanted start_embedded_timing_runtime.
    not_started,
    // The timing task held the engine for longer than the caller was willing to wait.
    // Distinct from a refusal: the geometry was never looked at, so retrying is sensible
    // where retrying a rejection is not.
    busy,
    // The engine looked at the geometry and refused it. The previous circuit is gone -
    // TimingEngine::configure resets on every failure path - so this leaves the device
    // timer-only rather than quietly timing against the old track.
    rejected,
    reconfigured,
};

struct EmbeddedTimingMetrics {
    std::uint64_t fixes_enqueued{0};
    std::uint64_t fixes_processed{0};
    std::uint64_t fix_queue_drops{0};
    std::uint64_t lap_events_emitted{0};
    std::uint64_t lap_event_queue_drops{0};
    std::uint64_t gate_events_emitted{0};
    std::uint64_t gate_records_emitted{0};
    std::uint64_t gate_record_queue_drops{0};
    std::uint64_t deadline_misses{0};
    std::int64_t maximum_processing_us{0};
    std::size_t fix_queue_high_water_mark{0};
    std::size_t lap_event_queue_high_water_mark{0};
    std::size_t gate_record_queue_high_water_mark{0};
    // Fixes discarded because the circuit changed under them. Counted rather than folded
    // into fix_queue_drops: a drop there means a consumer was too slow and is a fault,
    // while this is a deliberate discard and is not.
    std::uint64_t fixes_abandoned_on_reconfigure{0};
};

// These calls are implemented by the ESP-IDF runtime. The timing task owns the
// engine and never calls UI or storage code. Queue operations are non-blocking.
[[nodiscard]] bool start_embedded_timing_runtime(
    const TimingEngineConfig& config) noexcept;
[[nodiscard]] bool enqueue_timing_fix(const domain::GnssFix& fix) noexcept;
[[nodiscard]] bool try_receive_lap_event(domain::LapEvent& event) noexcept;
[[nodiscard]] bool try_receive_gate_crossing_record(
    GateCrossingDecision& decision) noexcept;
[[nodiscard]] EmbeddedTimingMetrics embedded_timing_metrics() noexcept;

// Points the running engine at a different circuit.
//
// Needed because start_embedded_timing_runtime refuses a second call, so without this the
// first circuit selected would be the only one the device could ever time - and the track
// carousel lets a driver change circuit at any time.
//
// Everything queued is discarded along with the old geometry. A fix measured against one
// circuit and evaluated against another is not a stale reading, it is a crossing computed
// across two different tracks, and the lap and gate queues are no better: their contents
// name a circuit that is no longer loaded.
[[nodiscard]] TimingReconfigureResult reconfigure_embedded_timing(
    const TimingEngineConfig& config) noexcept;

// The engine's own view of the lap in progress, as of the last fix it processed.
//
// Published by the task rather than read from the engine, because the engine belongs to
// that task and a caller reaching into it would be reading a structure being written. The
// copy is taken under the same spinlock as the metrics, so a UI tick asking for it cannot
// be made to wait on a fix being processed.
[[nodiscard]] LapStateSnapshot embedded_lap_snapshot() noexcept;

}  // namespace track_timer::timing
