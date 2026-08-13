#include "track_timer/simulator/scenario.hpp"

#include <algorithm>

namespace track_timer::simulator {
namespace {

domain::UiSnapshot initial_snapshot(const ScenarioId id) noexcept
{
    switch (id) {
    case ScenarioId::ready:
        return domain::UiSnapshot{
            30 * 60'000,
            domain::kUnavailableTime,
            domain::kUnavailableTime,
            domain::kUnavailableTime,
            0,
            domain::GnssHealth::searching,
            false,
            true,
        };
    case ScenarioId::active:
        return domain::UiSnapshot{
            15 * 60'000 + 27'000,
            1 * 60'000 + 42'638,
            1 * 60'000 + 43'112,
            1 * 60'000 + 42'985,
            7,
            domain::GnssHealth::good,
            true,
            true,
        };
    case ScenarioId::gnss_loss:
        return domain::UiSnapshot{
            4 * 60'000 + 51'000,
            52'440,
            1 * 60'000 + 40'205,
            1 * 60'000 + 39'880,
            8,
            domain::GnssHealth::stale,
            true,
            true,
        };
    case ScenarioId::storage_failure:
        return domain::UiSnapshot{
            8 * 60'000 + 12'000,
            18'760,
            1 * 60'000 + 41'010,
            1 * 60'000 + 39'880,
            9,
            domain::GnssHealth::good,
            true,
            false,
        };
    }
    return {};
}

}  // namespace

const char* scenario_name(const ScenarioId id) noexcept
{
    switch (id) {
    case ScenarioId::ready:
        return "ready";
    case ScenarioId::active:
        return "active";
    case ScenarioId::gnss_loss:
        return "gnss-loss";
    case ScenarioId::storage_failure:
        return "storage-failure";
    }
    return "ready";
}

bool parse_scenario(const std::string_view name, ScenarioId& id) noexcept
{
    for (const auto candidate : kAllScenarios) {
        if (name == scenario_name(candidate)) {
            id = candidate;
            return true;
        }
    }
    return false;
}

ScenarioId next_scenario(const ScenarioId id) noexcept
{
    const auto current = std::find(kAllScenarios.begin(), kAllScenarios.end(), id);
    if (current == kAllScenarios.end() || std::next(current) == kAllScenarios.end()) {
        return kAllScenarios.front();
    }
    return *std::next(current);
}

ScenarioPlayer::ScenarioPlayer(const ScenarioId id) noexcept : id_(id), snapshot_(initial_snapshot(id)) {}

void ScenarioPlayer::reset(const ScenarioId id) noexcept
{
    id_ = id;
    elapsed_ms_ = 0;
    snapshot_ = initial_snapshot(id);
}

void ScenarioPlayer::advance(const std::int64_t elapsed_ms) noexcept
{
    if (elapsed_ms <= 0) {
        return;
    }

    elapsed_ms_ += elapsed_ms;
    if (!snapshot_.session_active) {
        return;
    }

    snapshot_.session_remaining_ms -= elapsed_ms;
    if (snapshot_.current_lap_ms == domain::kUnavailableTime) {
        snapshot_.current_lap_ms = 0;
    }
    snapshot_.current_lap_ms += elapsed_ms;
}

ScenarioId ScenarioPlayer::id() const noexcept
{
    return id_;
}

std::int64_t ScenarioPlayer::elapsed_ms() const noexcept
{
    return elapsed_ms_;
}

const domain::UiSnapshot& ScenarioPlayer::snapshot() const noexcept
{
    return snapshot_;
}

}  // namespace track_timer::simulator
