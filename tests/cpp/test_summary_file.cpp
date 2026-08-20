#include "track_timer/logger/file_summary_store.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include <unistd.h>  // truncate(), for simulating a card pulled mid-write

namespace {

using namespace track_timer;

std::string temp_path(const char* const name)
{
    const auto* base = std::getenv("TMPDIR");
    std::string path = base != nullptr ? base : "/tmp";
    path += "/track_timer_";
    path += name;
    std::remove(path.c_str());
    return path;
}

logger::SessionSummaryV1 summary_of(const char* const id, const std::int64_t duration_ms)
{
    logger::SessionSummaryV1 summary{};
    summary.record_size_bytes = static_cast<std::uint16_t>(sizeof(summary));
    std::snprintf(summary.session_id.data(), summary.session_id.size(), "%s", id);
    summary.session_duration_ms = duration_ms;
    summary.completion_reason = logger::SessionCompletionReason::driver_stop;
    summary.integrity = logger::SummaryIntegrity::complete;
    summary.degraded_subsystems = logger::degraded_gnss;
    summary.peaks.total_g = 1.15F;
    summary.peaks.up_g = 0.4F;
    return summary;
}

// The point of the card: a driver's sessions outlive the power cycle.
void sessions_survive_a_reopen()
{
    const auto path = temp_path("roundtrip.bin");
    {
        logger::FileSummaryStore store;
        assert(store.open(path.c_str()) == logger::FileSummaryStore::OpenResult::no_file);
        assert(store.append(summary_of("S001", 60'000)));
        assert(store.append(summary_of("S002", 120'000)));
        assert(store.append(summary_of("S003", 180'000)));
        assert(store.persistent());
    }

    logger::FileSummaryStore reopened;
    assert(reopened.open(path.c_str()) == logger::FileSummaryStore::OpenResult::ready);
    std::size_t count = 0;
    assert(reopened.session_count(count) == logger::SummaryReadResult::ready);
    assert(count == 3);

    logger::SessionSummaryV1 read{};
    assert(reopened.read_summary(0, read) == logger::SummaryReadResult::ready);
    assert(std::strcmp(read.session_id.data(), "S003") == 0);  // newest first
    assert(read.session_duration_ms == 180'000);
    assert(read.peaks.total_g == 1.15F);
    assert(read.peaks.up_g == 0.4F);
    assert(reopened.scan_report().accepted == 3);
    assert(reopened.scan_report().skipped == 0);
    assert(!reopened.scan_report().truncated_tail);
    std::remove(path.c_str());
}

// A card pulled mid-write leaves a partial frame. The sessions already written must not go
// with it - that is the whole reason for framing each record.
void a_torn_final_frame_costs_only_that_record()
{
    const auto path = temp_path("torn.bin");
    {
        logger::FileSummaryStore store;
        (void)store.open(path.c_str());
        assert(store.append(summary_of("S001", 60'000)));
        assert(store.append(summary_of("S002", 120'000)));
    }
    // Chop the last frame in half, as an interrupted write would.
    auto* file = std::fopen(path.c_str(), "rb");
    std::fseek(file, 0, SEEK_END);
    const auto full = std::ftell(file);
    std::fclose(file);
    assert(truncate(path.c_str(), full - static_cast<long>(logger::kSummaryFrameSize / 2)) == 0);

    logger::FileSummaryStore reopened;
    assert(reopened.open(path.c_str()) == logger::FileSummaryStore::OpenResult::ready);
    std::size_t count = 0;
    assert(reopened.session_count(count) == logger::SummaryReadResult::ready);
    assert(count == 1);
    assert(reopened.scan_report().truncated_tail);

    logger::SessionSummaryV1 read{};
    assert(reopened.read_summary(0, read) == logger::SummaryReadResult::ready);
    assert(std::strcmp(read.session_id.data(), "S001") == 0);
    std::remove(path.c_str());
}

// Stopping at the first bad frame would throw away good sessions to protect a bad one.
void a_corrupt_frame_does_not_hide_the_ones_after_it()
{
    const auto path = temp_path("corrupt.bin");
    {
        logger::FileSummaryStore store;
        (void)store.open(path.c_str());
        assert(store.append(summary_of("S001", 60'000)));
        assert(store.append(summary_of("S002", 120'000)));
        assert(store.append(summary_of("S003", 180'000)));
    }
    // Flip a byte inside the middle frame's payload.
    auto* file = std::fopen(path.c_str(), "r+b");
    assert(file != nullptr);
    std::fseek(file, static_cast<long>(logger::kSummaryFrameSize + 40), SEEK_SET);
    int byte = std::fgetc(file);
    std::fseek(file, static_cast<long>(logger::kSummaryFrameSize + 40), SEEK_SET);
    std::fputc(byte ^ 0x5A, file);
    std::fclose(file);

    logger::FileSummaryStore reopened;
    assert(reopened.open(path.c_str()) == logger::FileSummaryStore::OpenResult::ready);
    assert(reopened.scan_report().skipped == 1);
    assert(reopened.scan_report().accepted == 2);

    std::size_t count = 0;
    (void)reopened.session_count(count);
    assert(count == 2);
    logger::SessionSummaryV1 read{};
    assert(reopened.read_summary(0, read) == logger::SummaryReadResult::ready);
    assert(std::strcmp(read.session_id.data(), "S003") == 0);  // the one after the damage
    std::remove(path.c_str());
}

// No card must never cost the driver the session they just drove.
void a_missing_card_still_keeps_the_session_in_review()
{
    logger::FileSummaryStore store;
    assert(store.open("/nonexistent-directory-for-tests/summaries.bin") ==
           logger::FileSummaryStore::OpenResult::no_file);

    // The write fails, and the session is still there to review.
    const auto appended = store.append(summary_of("S001", 60'000));
    assert(!appended);
    assert(!store.persistent());
    assert(store.write_failure_count() == 1);

    std::size_t count = 0;
    assert(store.session_count(count) == logger::SummaryReadResult::ready);
    assert(count == 1);
    logger::SessionSummaryV1 read{};
    assert(store.read_summary(0, read) == logger::SummaryReadResult::ready);
    assert(std::strcmp(read.session_id.data(), "S001") == 0);
}

// A card that has been in the device all season must not have to be held in RAM to be read.
void only_the_newest_sessions_are_kept_in_memory()
{
    const auto path = temp_path("long.bin");
    constexpr std::size_t kWritten = logger::MemorySummaryStore::kCapacity + 5;
    {
        logger::FileSummaryStore store;
        (void)store.open(path.c_str());
        for (std::size_t index = 0; index < kWritten; ++index) {
            char id[8]{};
            std::snprintf(id, sizeof(id), "S%03u", static_cast<unsigned>(index));
            assert(store.append(summary_of(id, 60'000)));
        }
    }

    logger::FileSummaryStore reopened;
    assert(reopened.open(path.c_str()) == logger::FileSummaryStore::OpenResult::ready);
    std::size_t count = 0;
    (void)reopened.session_count(count);
    assert(count == logger::MemorySummaryStore::kCapacity);

    // The window seeks to a frame boundary, so the newest record is intact rather than
    // starting halfway through one.
    logger::SessionSummaryV1 read{};
    assert(reopened.read_summary(0, read) == logger::SummaryReadResult::ready);
    char newest[8]{};
    std::snprintf(newest, sizeof(newest), "S%03u", static_cast<unsigned>(kWritten - 1));
    assert(std::strcmp(read.session_id.data(), newest) == 0);
    std::remove(path.c_str());
}

// A record that is intact on the wire can still be nonsense; a checksum cannot tell you a
// session lasted a negative length of time.
void framing_refuses_a_record_that_fails_its_own_validator()
{
    logger::SummaryFrame frame{};
    auto invalid = summary_of("S001", 60'000);
    invalid.session_overrun_ms = 120'000;  // longer than the session itself
    assert(!logger::encode_summary_frame(invalid, frame));

    auto valid = summary_of("S001", 60'000);
    assert(logger::encode_summary_frame(valid, frame));
    assert(frame.size == logger::kSummaryFrameSize);

    logger::SessionSummaryV1 decoded{};
    assert(logger::decode_summary_frame(frame.bytes.data(), frame.size, decoded) ==
           logger::FrameResult::ready);

    auto wrong_magic = frame;
    wrong_magic.bytes[0] = 'X';
    assert(logger::decode_summary_frame(wrong_magic.bytes.data(), wrong_magic.size, decoded) ==
           logger::FrameResult::bad_magic);

    auto wrong_version = frame;
    wrong_version.bytes[4] = static_cast<std::uint8_t>(logger::kLogFormatVersion + 1);
    assert(logger::decode_summary_frame(wrong_version.bytes.data(), wrong_version.size,
                                        decoded) == logger::FrameResult::unsupported_version);

    assert(logger::decode_summary_frame(frame.bytes.data(), logger::kSummaryFrameSize - 1,
                                        decoded) == logger::FrameResult::too_short);
}

}  // namespace

int main()
{
    sessions_survive_a_reopen();
    a_torn_final_frame_costs_only_that_record();
    a_corrupt_frame_does_not_hide_the_ones_after_it();
    a_missing_card_still_keeps_the_session_in_review();
    only_the_newest_sessions_are_kept_in_memory();
    framing_refuses_a_record_that_fails_its_own_validator();

    std::cout << "Session summaries on card: survive a reopen, a torn tail costs one record, "
                 "a corrupt frame does not hide later ones, and no card still leaves the "
                 "session reviewable passed\n";
    return 0;
}
