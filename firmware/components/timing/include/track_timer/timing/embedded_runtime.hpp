#pragma once

#include "track_timer/timing/engine.hpp"

#include <cstddef>
#include <cstdint>

namespace track_timer::timing {

inline constexpr std::int64_t kTimingFixDeadlineUs = 40'000;

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

}  // namespace track_timer::timing
