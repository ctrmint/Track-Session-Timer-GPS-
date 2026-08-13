#pragma once

#include "track_timer/domain/contracts.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::diagnostics {

inline constexpr std::uint16_t kDiagnosticsSnapshotVersion = 2;

enum class OverallState : std::uint8_t {
    normal,
    degraded,
    missing,
    recovered,
};

enum class SubsystemState : std::uint8_t {
    ready,
    degraded,
    unavailable,
    not_simulated,
};

enum class ResetReason : std::uint8_t {
    unknown,
    power_on,
    software,
    watchdog,
    brownout,
    panic,
};

enum class BackendKind : std::uint8_t {
    hardware,
    simulator,
    unavailable,
};

struct QueueSnapshot {
    std::size_t capacity{0};
    std::size_t depth{0};
    std::size_t high_water_mark{0};
    std::uint64_t dropped{0};
};

struct MemorySnapshot {
    SubsystemState state{SubsystemState::unavailable};
    std::uint64_t free_bytes{0};
    std::uint64_t total_bytes{0};
};

struct DiagnosticsSnapshot {
    std::uint16_t schema_version{kDiagnosticsSnapshotVersion};
    OverallState overall{OverallState::normal};
    std::array<char, 32> firmware_version{};
    std::uint64_t uptime_ms{0};
    ResetReason reset_reason{ResetReason::unknown};
    MemorySnapshot internal_ram{};
    MemorySnapshot psram{};
    BackendKind backend{BackendKind::unavailable};

    SubsystemState gnss{SubsystemState::unavailable};
    std::uint16_t gnss_rate_hz{0};
    domain::FixType gnss_fix_type{domain::FixType::no_fix};
    std::uint8_t gnss_satellites{0};
    float gnss_horizontal_accuracy_m{-1.0F};
    QueueSnapshot gnss_queue{};
    std::uint32_t gnss_recoveries{0};

    SubsystemState storage{SubsystemState::unavailable};
    std::uint64_t storage_available_bytes{0};
    QueueSnapshot logger_queue{};
    std::uint64_t logger_write_failures{0};
    std::uint32_t storage_recoveries{0};
    std::int64_t logger_p95_latency_us{0};

    SubsystemState imu{SubsystemState::unavailable};
    QueueSnapshot imu_queue{};
    std::uint64_t imu_samples{0};
    SubsystemState rtc{SubsystemState::unavailable};
    SubsystemState touch{SubsystemState::unavailable};
    QueueSnapshot touch_queue{};
    SubsystemState display{SubsystemState::unavailable};
    std::uint32_t display_frame_count{0};
    std::uint32_t display_average_update_us{0};
    std::uint32_t display_maximum_update_us{0};
    std::size_t display_maximum_lvgl_bytes{0};
    std::uint16_t display_orientation_degrees{0};
    std::uint8_t display_brightness_percent{100};
    std::int8_t display_shift_x{0};
    std::int8_t display_shift_y{0};
    bool display_dimmed{false};
};

static_assert(std::is_trivially_copyable_v<QueueSnapshot>);
static_assert(std::is_trivially_copyable_v<DiagnosticsSnapshot>);

}  // namespace track_timer::diagnostics
