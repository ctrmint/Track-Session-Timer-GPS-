#pragma once

#include "track_timer/diagnostics/snapshot.hpp"
#include "track_timer/logger/async_logger.hpp"
#include "track_timer/simulator/device_backends.hpp"
#include "track_timer/ui/foundation.hpp"

#include <cstdint>
#include <string_view>

namespace track_timer::simulator {

enum class DiagnosticsFixtureId : std::uint8_t {
    normal,
    degraded,
    missing,
    recovery,
};

[[nodiscard]] diagnostics::DiagnosticsSnapshot make_diagnostics_snapshot(
    DiagnosticsFixtureId fixture, std::uint64_t uptime_ms,
    const DeviceDiagnostics& device, const board::StorageStatus& storage,
    const logger::LoggerMetrics& logger, const ui::RenderMetrics& display) noexcept;
[[nodiscard]] bool parse_diagnostics_fixture(std::string_view name,
                                             DiagnosticsFixtureId& fixture) noexcept;
[[nodiscard]] const char* diagnostics_fixture_name(DiagnosticsFixtureId fixture) noexcept;

}  // namespace track_timer::simulator
