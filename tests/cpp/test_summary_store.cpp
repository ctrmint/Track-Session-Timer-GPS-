#include "track_timer/logger/memory_summary_store.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>

namespace {

using namespace track_timer;

logger::SessionSummaryV1 summary_of(const char* const id, const std::int64_t duration_ms,
                                    const float total_g = 0.9F)
{
    logger::SessionSummaryV1 summary{};
    summary.record_size_bytes = static_cast<std::uint16_t>(sizeof(summary));
    std::snprintf(summary.session_id.data(), summary.session_id.size(), "%s", id);
    summary.session_duration_ms = duration_ms;
    summary.session_overrun_ms = 0;
    summary.completion_reason = logger::SessionCompletionReason::driver_stop;
    summary.integrity = logger::SummaryIntegrity::complete;
    summary.degraded_subsystems = logger::degraded_gnss;
    summary.peaks.total_g = total_g;
    return summary;
}

// A driver wants the session they just finished, so it has to be the one Review opens on.
void the_newest_session_is_first()
{
    logger::MemorySummaryStore store;
    std::size_t count = 0;
    assert(store.session_count(count) == logger::SummaryReadResult::empty);
    assert(count == 0);

    assert(store.record(summary_of("S001", 20 * 60'000)));
    assert(store.record(summary_of("S002", 15 * 60'000)));
    assert(store.session_count(count) == logger::SummaryReadResult::ready);
    assert(count == 2);

    logger::SessionSummaryV1 read{};
    assert(store.read_summary(0, read) == logger::SummaryReadResult::ready);
    assert(std::strcmp(read.session_id.data(), "S002") == 0);
    assert(store.read_summary(1, read) == logger::SummaryReadResult::ready);
    assert(std::strcmp(read.session_id.data(), "S001") == 0);
}

// The oldest falls off rather than the newest being refused: the last session matters far
// more than the ninth one back.
void the_oldest_session_is_the_one_lost()
{
    logger::MemorySummaryStore store;
    for (std::size_t index = 0; index < logger::MemorySummaryStore::kCapacity + 3; ++index) {
        char id[8]{};
        std::snprintf(id, sizeof(id), "S%03u", static_cast<unsigned>(index));
        assert(store.record(summary_of(id, 60'000)));
    }
    std::size_t count = 0;
    assert(store.session_count(count) == logger::SummaryReadResult::ready);
    assert(count == logger::MemorySummaryStore::kCapacity);

    logger::SessionSummaryV1 read{};
    assert(store.read_summary(0, read) == logger::SummaryReadResult::ready);
    assert(std::strcmp(read.session_id.data(), "S010") == 0);  // the most recent
    assert(store.read_summary(logger::MemorySummaryStore::kCapacity, read) ==
           logger::SummaryReadResult::corrupt);
}

// A malformed record must not reach Review and be rendered as though it were real.
void a_summary_that_fails_its_own_validator_is_refused()
{
    logger::MemorySummaryStore store;

    auto no_identifier = summary_of("", 60'000);
    assert(!store.record(no_identifier));

    auto overrun_exceeds_duration = summary_of("S001", 60'000);
    overrun_exceeds_duration.session_overrun_ms = 120'000;
    assert(!store.record(overrun_exceeds_duration));

    // A peak beyond anything a car can pull says the reading is wrong, not remarkable.
    auto impossible_peak = summary_of("S002", 60'000, 40.0F);
    assert(!store.record(impossible_peak));

    // A negative peak is not a magnitude at all.
    auto negative_peak = summary_of("S003", 60'000);
    negative_peak.peaks.braking_g = -1.0F;
    assert(!store.record(negative_peak));

    assert(store.rejected_count() == 4);
    std::size_t count = 0;
    assert(store.session_count(count) == logger::SummaryReadResult::empty);
}

// Every axis survives the round trip, which is the whole point of recording them.
void the_peaks_are_carried_through()
{
    logger::MemorySummaryStore store;
    auto summary = summary_of("S001", 60'000);
    summary.peaks.acceleration_g = 0.62F;
    summary.peaks.braking_g = 1.14F;
    summary.peaks.left_g = 0.98F;
    summary.peaks.right_g = 1.02F;
    summary.peaks.up_g = 0.44F;
    summary.peaks.down_g = 0.71F;
    summary.peaks.total_g = 1.21F;
    assert(store.record(summary));

    logger::SessionSummaryV1 read{};
    assert(store.read_summary(0, read) == logger::SummaryReadResult::ready);
    assert(read.peaks.acceleration_g == 0.62F);
    assert(read.peaks.braking_g == 1.14F);
    assert(read.peaks.left_g == 0.98F);
    assert(read.peaks.right_g == 1.02F);
    assert(read.peaks.up_g == 0.44F);
    assert(read.peaks.down_g == 0.71F);
    assert(read.peaks.total_g == 1.21F);
}

// No receiver means no laps, which is a session with nothing to list rather than a fault.
void a_session_without_laps_reads_as_an_empty_page()
{
    logger::MemorySummaryStore store;
    assert(store.record(summary_of("S001", 60'000)));

    logger::SessionSummaryV1 read{};
    assert(store.read_summary(0, read) == logger::SummaryReadResult::ready);

    logger::SummaryLapPage page{};
    assert(store.read_lap_page(read.session_id, 0, page) == logger::SummaryReadResult::ready);
    assert(page.count == 0);
    assert(page.total_count == 0);
}

void clearing_empties_it()
{
    logger::MemorySummaryStore store;
    assert(store.record(summary_of("S001", 60'000)));
    store.clear();
    std::size_t count = 0;
    assert(store.session_count(count) == logger::SummaryReadResult::empty);
    assert(store.rejected_count() == 0);
}

}  // namespace

int main()
{
    the_newest_session_is_first();
    the_oldest_session_is_the_one_lost();
    a_summary_that_fails_its_own_validator_is_refused();
    the_peaks_are_carried_through();
    a_session_without_laps_reads_as_an_empty_page();
    clearing_empties_it();

    std::cout << "Session summary store: newest first, bounded history, refusal of records "
                 "that fail their own validator, and peaks carried on every axis passed\n";
    return 0;
}
