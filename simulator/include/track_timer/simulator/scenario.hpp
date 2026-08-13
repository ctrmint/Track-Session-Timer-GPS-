#pragma once

#include "track_timer/domain/contracts.hpp"
#include "track_timer/logger/async_logger.hpp"
#include "track_timer/simulator/device_backends.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace track_timer::simulator {

enum class ScenarioId : std::uint8_t {
    ready,
    active,
    gnss_loss,
    storage_failure,
};

inline constexpr std::array<ScenarioId, 4> kAllScenarios{
    ScenarioId::ready,
    ScenarioId::active,
    ScenarioId::gnss_loss,
    ScenarioId::storage_failure,
};

[[nodiscard]] const char* scenario_name(ScenarioId id) noexcept;
[[nodiscard]] bool parse_scenario(std::string_view name, ScenarioId& id) noexcept;
[[nodiscard]] ScenarioId next_scenario(ScenarioId id) noexcept;

class ScenarioPlayer {
  public:
    explicit ScenarioPlayer(ScenarioId id,
                            GnssFixture fixture = make_synthetic_gnss_fixture(),
                            GnssReplayRate rate = GnssReplayRate::hz25);

    void reset(ScenarioId id) noexcept;
    void advance(std::int64_t elapsed_ms) noexcept;

    [[nodiscard]] ScenarioId id() const noexcept;
    [[nodiscard]] std::int64_t elapsed_ms() const noexcept;
    [[nodiscard]] const domain::UiSnapshot& snapshot() const noexcept;
    [[nodiscard]] DeviceDiagnostics diagnostics() const noexcept;
    [[nodiscard]] logger::LoggerMetrics logger_metrics() const noexcept;
    [[nodiscard]] SimulatedDevice& device() noexcept;

    ScenarioPlayer(const ScenarioPlayer&) = delete;
    ScenarioPlayer& operator=(const ScenarioPlayer&) = delete;
    ScenarioPlayer(ScenarioPlayer&&) = delete;
    ScenarioPlayer& operator=(ScenarioPlayer&&) = delete;

  private:
    void apply_fault_schedule() noexcept;
    void consume_inputs() noexcept;
    [[nodiscard]] std::int64_t next_fault_boundary_ms() const noexcept;

    ScenarioId id_;
    std::int64_t elapsed_ms_{0};
    domain::UiSnapshot snapshot_{};
    SimulatedDevice device_;
    logger::AsyncLogger logger_;
};

}  // namespace track_timer::simulator
