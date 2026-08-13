#include "track_timer/simulator/summary_fixtures.hpp"

#include <algorithm>
#include <cstdio>

namespace track_timer::simulator {
namespace {

logger::SessionSummaryV1 make_summary(const char* session_id, const std::uint32_t lap_count,
                                      const std::uint32_t best_lap,
                                      const std::int64_t best_lap_ns) noexcept
{
    logger::SessionSummaryV1 summary{};
    summary.record_size_bytes = sizeof(summary);
    std::snprintf(summary.session_id.data(), summary.session_id.size(), "%s", session_id);
    summary.session_duration_ms = 1'925'000;
    summary.session_overrun_ms = 125'000;
    summary.completion_reason = logger::SessionCompletionReason::driver_stop;
    summary.lap_count = lap_count;
    summary.best_lap_index = best_lap;
    summary.best_lap_duration_ns = best_lap_ns;
    summary.source_first_record_sequence = 1;
    summary.source_last_record_sequence = 10'000;
    summary.source_gnss_record_count = 9'500;
    summary.source_accepted_fix_count = 9'480;
    summary.source_rejected_fix_count = 20;
    summary.source_event_record_count = lap_count + 2;
    return summary;
}

logger::SummaryLapRecordV1 make_lap(const std::uint32_t index,
                                    const std::int64_t duration_ns) noexcept
{
    logger::SummaryLapRecordV1 lap{};
    lap.record_size_bytes = sizeof(lap);
    lap.lap_index = index;
    lap.lap_duration_ns = duration_ns;
    lap.event_record_sequence = index * 1'000;
    lap.segment_sequence_0 = index * 900;
    lap.segment_sequence_1 = lap.segment_sequence_0 + 900;
    return lap;
}

bool same_id(const std::array<char, logger::kSessionIdentifierCapacity>& left,
             const std::array<char, logger::kSessionIdentifierCapacity>& right) noexcept
{
    return left == right;
}

}  // namespace

SummaryFixtureProvider::SummaryFixtureProvider(const SummaryFixtureId fixture) noexcept
    : fixture_(fixture)
{
    summaries_[0] = make_summary("2026-08-13T142500Z", 7, 5, 61'842'000'000LL);
    summaries_[0].degraded_subsystems = logger::degraded_gnss | logger::degraded_imu;
    lap_counts_[0] = 7;
    constexpr std::array<std::int64_t, 7> latest_laps{
        64'210'000'000LL, 63'400'000'000LL, 62'955'000'000LL, 62'100'000'000LL,
        61'842'000'000LL, 62'008'000'000LL, 61'990'000'000LL,
    };
    for (std::size_t index = 0; index < latest_laps.size(); ++index) {
        laps_[0][index] = make_lap(static_cast<std::uint32_t>(index + 1), latest_laps[index]);
    }

    summaries_[1] = make_summary("2026-08-12T103000Z", 3, 2, 62'050'000'000LL);
    summaries_[1].session_duration_ms = 1'800'000;
    summaries_[1].session_overrun_ms = 0;
    lap_counts_[1] = 3;
    constexpr std::array<std::int64_t, 3> older_laps{
        62'500'000'000LL, 62'050'000'000LL, 62'300'000'000LL,
    };
    for (std::size_t index = 0; index < older_laps.size(); ++index) {
        laps_[1][index] = make_lap(static_cast<std::uint32_t>(index + 1), older_laps[index]);
    }

    if (fixture_ == SummaryFixtureId::partial) {
        summaries_[0].integrity = logger::SummaryIntegrity::partial_log;
        summaries_[0].degraded_subsystems |= logger::degraded_storage;
        summaries_[0].logger_dropped_record_count = 12;
        summaries_[0].logger_write_failure_count = 2;
    }
    else if (fixture_ == SummaryFixtureId::corrupt) {
        summaries_[0].record_size_bytes = 0;
    }
    else if (fixture_ == SummaryFixtureId::unsupported) {
        summaries_[0].schema_version = logger::kLogFormatVersion + 1;
    }
}

logger::SummaryReadResult SummaryFixtureProvider::session_count(std::size_t& count) noexcept
{
    count = 0;
    if (fixture_ == SummaryFixtureId::missing) {
        return logger::SummaryReadResult::storage_unavailable;
    }
    if (fixture_ == SummaryFixtureId::empty) {
        return logger::SummaryReadResult::empty;
    }
    count = fixture_ == SummaryFixtureId::complete ? summaries_.size() : 1;
    return logger::SummaryReadResult::ready;
}

logger::SummaryReadResult SummaryFixtureProvider::read_summary(
    const std::size_t history_index, logger::SessionSummaryV1& summary) noexcept
{
    const auto count = fixture_ == SummaryFixtureId::complete ? summaries_.size() : 1;
    if (fixture_ == SummaryFixtureId::missing) {
        return logger::SummaryReadResult::storage_unavailable;
    }
    if (fixture_ == SummaryFixtureId::empty || history_index >= count) {
        return logger::SummaryReadResult::empty;
    }
    summary = summaries_[history_index];
    return logger::SummaryReadResult::ready;
}

logger::SummaryReadResult SummaryFixtureProvider::read_lap_page(
    const std::array<char, logger::kSessionIdentifierCapacity>& session_id,
    const std::size_t offset, logger::SummaryLapPage& page) noexcept
{
    page = {};
    std::size_t session_index = summaries_.size();
    for (std::size_t index = 0; index < summaries_.size(); ++index) {
        if (same_id(session_id, summaries_[index].session_id)) {
            session_index = index;
            break;
        }
    }
    if (session_index == summaries_.size() || offset > lap_counts_[session_index]) {
        return logger::SummaryReadResult::corrupt;
    }
    page.offset = offset;
    page.total_count = lap_counts_[session_index];
    page.count = std::min(logger::kSummaryLapPageCapacity,
                          lap_counts_[session_index] - offset);
    for (std::size_t index = 0; index < page.count; ++index) {
        page.laps[index] = laps_[session_index][offset + index];
    }
    return page.count == 0 ? logger::SummaryReadResult::empty
                           : logger::SummaryReadResult::ready;
}

SummaryFixtureId SummaryFixtureProvider::fixture() const noexcept
{
    return fixture_;
}

bool parse_summary_fixture(const std::string_view name, SummaryFixtureId& fixture) noexcept
{
    if (name == "complete") {
        fixture = SummaryFixtureId::complete;
    }
    else if (name == "partial") {
        fixture = SummaryFixtureId::partial;
    }
    else if (name == "empty") {
        fixture = SummaryFixtureId::empty;
    }
    else if (name == "missing") {
        fixture = SummaryFixtureId::missing;
    }
    else if (name == "corrupt") {
        fixture = SummaryFixtureId::corrupt;
    }
    else if (name == "unsupported") {
        fixture = SummaryFixtureId::unsupported;
    }
    else {
        return false;
    }
    return true;
}

const char* summary_fixture_name(const SummaryFixtureId fixture) noexcept
{
    switch (fixture) {
    case SummaryFixtureId::complete:
        return "complete";
    case SummaryFixtureId::partial:
        return "partial";
    case SummaryFixtureId::empty:
        return "empty";
    case SummaryFixtureId::missing:
        return "missing";
    case SummaryFixtureId::corrupt:
        return "corrupt";
    case SummaryFixtureId::unsupported:
        return "unsupported";
    }
    return "corrupt";
}

}  // namespace track_timer::simulator
