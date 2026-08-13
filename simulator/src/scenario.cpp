#include "track_timer/simulator/scenario.hpp"

#include <algorithm>
#include <limits>
#include <utility>

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
    case ScenarioId::lap_faster:
        return domain::UiSnapshot{
            15 * 60'000,
            54'000,
            1 * 60'000 + 41'000,
            1 * 60'000 + 40'500,
            7,
            domain::GnssHealth::good,
            true,
            true,
        };
    case ScenarioId::lap_slower:
        return domain::UiSnapshot{
            15 * 60'000,
            54'000,
            1 * 60'000 + 41'000,
            1 * 60'000 + 40'500,
            7,
            domain::GnssHealth::good,
            true,
            true,
        };
    case ScenarioId::lap_unavailable_best:
        return domain::UiSnapshot{
            15 * 60'000,
            54'000,
            domain::kUnavailableTime,
            domain::kUnavailableTime,
            1,
            domain::GnssHealth::good,
            true,
            true,
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
    case ScenarioId::lap_faster:
        return "lap-faster";
    case ScenarioId::lap_slower:
        return "lap-slower";
    case ScenarioId::lap_unavailable_best:
        return "lap-unavailable-best";
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

ScenarioPlayer::ScenarioPlayer(const ScenarioId id, GnssFixture fixture, const GnssReplayRate rate)
    : id_(id), snapshot_(initial_snapshot(id)), device_(std::move(fixture), rate),
      logger_(device_.storage())
{
    apply_fault_schedule();
}

void ScenarioPlayer::reset(const ScenarioId id) noexcept
{
    id_ = id;
    elapsed_ms_ = 0;
    lap_emitted_ = false;
    snapshot_ = initial_snapshot(id);
    device_.reset();
    logger_.reset();
    apply_fault_schedule();
    apply_lap_schedule();
}

void ScenarioPlayer::advance(const std::int64_t elapsed_ms) noexcept
{
    if (elapsed_ms <= 0) {
        return;
    }

    auto remaining_ms = elapsed_ms;
    while (remaining_ms > 0) {
        apply_fault_schedule();
        apply_lap_schedule();
        const auto boundary_ms = next_fault_boundary_ms();
        const auto until_boundary_ms = boundary_ms > elapsed_ms_ ? boundary_ms - elapsed_ms_
                                                                  : remaining_ms;
        const auto step_ms = std::min(remaining_ms, until_boundary_ms);
        if (snapshot_.session_active) {
            snapshot_.session_remaining_ms -= step_ms;
            if (snapshot_.current_lap_ms == domain::kUnavailableTime) {
                snapshot_.current_lap_ms = 0;
            }
            snapshot_.current_lap_ms += step_ms;
        }
        device_.advance(step_ms * 1'000);
        consume_inputs();
        elapsed_ms_ += step_ms;
        remaining_ms -= step_ms;
    }
    apply_fault_schedule();
    apply_lap_schedule();
    consume_inputs();
}

void ScenarioPlayer::stop_session() noexcept
{
    snapshot_.session_active = false;
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

DeviceDiagnostics ScenarioPlayer::diagnostics() const noexcept
{
    return device_.diagnostics();
}

logger::LoggerMetrics ScenarioPlayer::logger_metrics() const noexcept
{
    return logger_.metrics();
}

SimulatedDevice& ScenarioPlayer::device() noexcept
{
    return device_;
}

void ScenarioPlayer::apply_fault_schedule() noexcept
{
    auto gnss_mode = GnssMode::normal;
    auto storage_mode = StorageMode::ready;
    if (id_ == ScenarioId::ready) {
        gnss_mode = GnssMode::loss;
    }
    else if (id_ == ScenarioId::gnss_loss) {
        if (elapsed_ms_ < 2'000) {
            gnss_mode = GnssMode::loss;
        }
        else if (elapsed_ms_ < 3'000) {
            gnss_mode = GnssMode::stale;
        }
        else if (elapsed_ms_ < 4'000) {
            gnss_mode = GnssMode::corrupt;
        }
    }
    else if (id_ == ScenarioId::storage_failure) {
        if (elapsed_ms_ < 1'000) {
            storage_mode = StorageMode::missing;
        }
        else if (elapsed_ms_ < 2'000) {
            storage_mode = StorageMode::full;
        }
        else if (elapsed_ms_ < 3'000) {
            storage_mode = StorageMode::slow;
        }
        else if (elapsed_ms_ < 4'000) {
            storage_mode = StorageMode::write_failed;
        }
    }
    device_.gnss().set_mode(gnss_mode);
    device_.storage().set_mode(storage_mode);
}

void ScenarioPlayer::apply_lap_schedule() noexcept
{
    if (lap_emitted_ || elapsed_ms_ < 250) {
        return;
    }
    switch (id_) {
    case ScenarioId::lap_faster:
        snapshot_.previous_lap_ms = 1 * 60'000 + 39'750;
        snapshot_.best_lap_ms = snapshot_.previous_lap_ms;
        break;
    case ScenarioId::lap_slower:
        snapshot_.previous_lap_ms = 1 * 60'000 + 42'250;
        break;
    case ScenarioId::lap_unavailable_best:
        snapshot_.previous_lap_ms = 1 * 60'000 + 41'500;
        snapshot_.best_lap_ms = snapshot_.previous_lap_ms;
        break;
    case ScenarioId::ready:
    case ScenarioId::active:
    case ScenarioId::gnss_loss:
    case ScenarioId::storage_failure:
        return;
    }
    ++snapshot_.lap_index;
    snapshot_.current_lap_ms = 0;
    lap_emitted_ = true;
}

void ScenarioPlayer::consume_inputs() noexcept
{
    if (device_.gnss().mode() == GnssMode::loss) {
        snapshot_.gnss_health = id_ == ScenarioId::ready ? domain::GnssHealth::searching
                                                         : domain::GnssHealth::stale;
    }

    domain::GnssFix fix{};
    while (device_.gnss().try_read(fix)) {
        if (fix.reject_reason == domain::FixRejectReason::stale) {
            snapshot_.gnss_health = domain::GnssHealth::stale;
        }
        else if (!fix.accepted_for_timing) {
            snapshot_.gnss_health = domain::GnssHealth::poor;
        }
        else {
            snapshot_.gnss_health = domain::GnssHealth::good;
        }

        domain::LogRecord record{};
        record.ordering_monotonic_us = fix.arrival_monotonic_us;
        record.sequence_number = fix.sequence_number;
        record.payload_size = static_cast<std::uint16_t>(sizeof(fix.sequence_number));
        record.type = domain::LogRecordType::gnss_fix;
        (void)logger_.enqueue(record);
    }

    board::ImuSample imu_sample{};
    while (device_.imu().try_read(imu_sample)) {
        (void)imu_sample;
    }

    (void)logger_.service(device_.clock().now_us());

    const auto storage_health = device_.storage().status().health;
    snapshot_.logging_available = storage_health == board::StorageHealth::ready ||
                                  storage_health == board::StorageHealth::degraded;
}

std::int64_t ScenarioPlayer::next_fault_boundary_ms() const noexcept
{
    constexpr auto kNoBoundary = std::numeric_limits<std::int64_t>::max();
    if (!lap_emitted_ && (id_ == ScenarioId::lap_faster || id_ == ScenarioId::lap_slower ||
                          id_ == ScenarioId::lap_unavailable_best)) {
        return 250;
    }
    if (id_ == ScenarioId::gnss_loss) {
        for (const auto boundary : {2'000, 3'000, 4'000}) {
            if (elapsed_ms_ < boundary) {
                return boundary;
            }
        }
    }
    else if (id_ == ScenarioId::storage_failure) {
        for (const auto boundary : {1'000, 2'000, 3'000, 4'000}) {
            if (elapsed_ms_ < boundary) {
                return boundary;
            }
        }
    }
    return kNoBoundary;
}

}  // namespace track_timer::simulator
