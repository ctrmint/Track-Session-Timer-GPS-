#include "track_timer/simulator/diagnostics_fixtures.hpp"

#include <algorithm>
#include <cstdio>

namespace track_timer::simulator {
namespace {

diagnostics::QueueSnapshot queue_snapshot(const QueueMetrics& source) noexcept
{
    return diagnostics::QueueSnapshot{
        source.capacity,
        source.depth,
        source.high_water_mark,
        source.dropped,
    };
}

diagnostics::QueueSnapshot queue_snapshot(const logger::LoggerQueueMetrics& source) noexcept
{
    return diagnostics::QueueSnapshot{
        source.capacity,
        source.depth,
        source.high_water_mark,
        source.dropped(),
    };
}

}  // namespace

diagnostics::DiagnosticsSnapshot make_diagnostics_snapshot(
    const DiagnosticsFixtureId fixture, const std::uint64_t uptime_ms,
    const DeviceDiagnostics& device, const board::StorageStatus& storage,
    const logger::LoggerMetrics& logger, const ui::RenderMetrics& display) noexcept
{
    diagnostics::DiagnosticsSnapshot snapshot{};
    std::snprintf(snapshot.firmware_version.data(), snapshot.firmware_version.size(),
                  "simulator-v1");
    snapshot.uptime_ms = uptime_ms;
    snapshot.reset_reason = diagnostics::ResetReason::power_on;
    snapshot.internal_ram = {diagnostics::SubsystemState::ready, 192U * 1'024U,
                             320U * 1'024U};
    snapshot.psram = {diagnostics::SubsystemState::not_simulated, 0, 0};
    snapshot.backend = diagnostics::BackendKind::simulator;

    snapshot.gnss = diagnostics::SubsystemState::ready;
    snapshot.gnss_rate_hz = 25;
    snapshot.gnss_fix_type = domain::FixType::fix_3d;
    snapshot.gnss_satellites = 14;
    snapshot.gnss_horizontal_accuracy_m = 0.7F;
    snapshot.gnss_queue = queue_snapshot(device.gnss.queue);
    snapshot.gnss_recoveries = device.gnss.recoveries;

    snapshot.storage = diagnostics::SubsystemState::ready;
    snapshot.storage_available_bytes = storage.available_bytes;
    snapshot.logger_queue = queue_snapshot(logger.queue);
    snapshot.logger_write_failures = logger.failed_batch_attempts;
    snapshot.storage_recoveries = device.storage.recoveries;
    snapshot.logger_p95_latency_us = logger.latency.p95_upper_bound_us;

    snapshot.imu = diagnostics::SubsystemState::ready;
    snapshot.imu_queue = queue_snapshot(device.imu_queue);
    snapshot.imu_samples = device.imu_emitted;
    snapshot.rtc = diagnostics::SubsystemState::ready;
    snapshot.touch = diagnostics::SubsystemState::ready;
    snapshot.touch_queue = queue_snapshot(device.touch_queue);
    snapshot.display = diagnostics::SubsystemState::ready;
    snapshot.display_frame_count = display.frame_count;
    snapshot.display_average_update_us = display.average_render_us();
    snapshot.display_maximum_update_us = display.maximum_render_us;
    snapshot.display_maximum_lvgl_bytes = display.maximum_lvgl_bytes;

    switch (fixture) {
    case DiagnosticsFixtureId::normal:
        snapshot.overall = diagnostics::OverallState::normal;
        break;
    case DiagnosticsFixtureId::degraded:
        snapshot.overall = diagnostics::OverallState::degraded;
        snapshot.gnss = diagnostics::SubsystemState::degraded;
        snapshot.gnss_satellites = 5;
        snapshot.gnss_horizontal_accuracy_m = 4.8F;
        snapshot.gnss_queue.dropped = std::max<std::uint64_t>(2, snapshot.gnss_queue.dropped);
        snapshot.storage = diagnostics::SubsystemState::degraded;
        snapshot.logger_write_failures =
            std::max<std::uint64_t>(1, snapshot.logger_write_failures);
        snapshot.imu = diagnostics::SubsystemState::degraded;
        break;
    case DiagnosticsFixtureId::missing:
        snapshot.overall = diagnostics::OverallState::missing;
        snapshot.gnss = diagnostics::SubsystemState::unavailable;
        snapshot.gnss_rate_hz = 0;
        snapshot.gnss_fix_type = domain::FixType::no_fix;
        snapshot.gnss_satellites = 0;
        snapshot.gnss_horizontal_accuracy_m = -1.0F;
        snapshot.storage = diagnostics::SubsystemState::unavailable;
        snapshot.storage_available_bytes = 0;
        snapshot.imu = diagnostics::SubsystemState::unavailable;
        break;
    case DiagnosticsFixtureId::recovery:
        snapshot.overall = diagnostics::OverallState::recovered;
        snapshot.gnss_recoveries = std::max<std::uint32_t>(2, snapshot.gnss_recoveries);
        snapshot.storage_recoveries =
            std::max<std::uint32_t>(2, snapshot.storage_recoveries);
        snapshot.gnss_queue.dropped = std::max<std::uint64_t>(1, snapshot.gnss_queue.dropped);
        snapshot.logger_write_failures =
            std::max<std::uint64_t>(3, snapshot.logger_write_failures);
        break;
    }
    return snapshot;
}

bool parse_diagnostics_fixture(const std::string_view name,
                               DiagnosticsFixtureId& fixture) noexcept
{
    if (name == "normal") {
        fixture = DiagnosticsFixtureId::normal;
    }
    else if (name == "degraded") {
        fixture = DiagnosticsFixtureId::degraded;
    }
    else if (name == "missing") {
        fixture = DiagnosticsFixtureId::missing;
    }
    else if (name == "recovery") {
        fixture = DiagnosticsFixtureId::recovery;
    }
    else {
        return false;
    }
    return true;
}

const char* diagnostics_fixture_name(const DiagnosticsFixtureId fixture) noexcept
{
    switch (fixture) {
    case DiagnosticsFixtureId::normal:
        return "normal";
    case DiagnosticsFixtureId::degraded:
        return "degraded";
    case DiagnosticsFixtureId::missing:
        return "missing";
    case DiagnosticsFixtureId::recovery:
        return "recovery";
    }
    return "degraded";
}

}  // namespace track_timer::simulator
