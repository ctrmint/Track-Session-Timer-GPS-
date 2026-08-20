#include "track_timer/logger/file_summary_store.hpp"

#include <array>
#include <algorithm>
#include <cstdio>

namespace track_timer::logger {
namespace {

// Only the newest sessions need to be readable, so a card that has been in the device all
// season is not read into RAM in its entirety.
constexpr std::size_t kFramesRead = MemorySummaryStore::kCapacity;

}  // namespace

FileSummaryStore::OpenResult FileSummaryStore::open(const char* const path) noexcept
{
    cache_.clear();
    scan_ = {};
    path_ = path;
    persistent_ = false;
    if (path == nullptr) {
        return OpenResult::unreadable;
    }

    auto* file = std::fopen(path, "rb");
    if (file == nullptr) {
        // No file yet is the ordinary state of a new card, and is not a failure: the first
        // append creates it. Whether the path is writable is answered then, not now.
        persistent_ = true;
        return OpenResult::no_file;
    }

    if (std::fseek(file, 0, SEEK_END) != 0) {
        (void)std::fclose(file);
        return OpenResult::unreadable;
    }
    const auto end = std::ftell(file);
    if (end < 0) {
        (void)std::fclose(file);
        return OpenResult::unreadable;
    }

    const auto total = static_cast<std::size_t>(end);
    const auto window = kFramesRead * kSummaryFrameSize;
    // Frames are a fixed size and only ever appended, so any multiple of that size is a
    // frame boundary. Seeking to one keeps the tail aligned however long the file is.
    const auto start = total > window ? ((total - window) / kSummaryFrameSize) * kSummaryFrameSize
                                      : 0U;
    if (std::fseek(file, static_cast<long>(start), SEEK_SET) != 0) {
        (void)std::fclose(file);
        return OpenResult::unreadable;
    }

    // Fixed storage, like everywhere else here: no dynamic allocation, and the window is
    // bounded by design so the buffer can be sized for the worst case up front. About 1.2 KB
    // on the stack, against a 24 KB task.
    std::array<std::uint8_t, kFramesRead * kSummaryFrameSize> buffer{};
    const auto wanted = std::min(buffer.size(), total - start);
    const auto read = wanted == 0 ? 0U : std::fread(buffer.data(), 1, wanted, file);
    (void)std::fclose(file);

    // Oldest first out of the file, so the newest ends up at the front of the cache.
    scan_ = scan_summary_frames(buffer.data(), read,
                                [this](const SessionSummaryV1& summary) {
                                    (void)cache_.record(summary);
                                });
    persistent_ = true;
    return OpenResult::ready;
}

bool FileSummaryStore::append(const SessionSummaryV1& summary) noexcept
{
    // The cache first, so a session survives in Review even when the card refuses it.
    const auto cached = cache_.record(summary);
    if (!cached) {
        return false;
    }
    if (path_ == nullptr) {
        return false;
    }

    SummaryFrame frame{};
    if (!encode_summary_frame(summary, frame)) {
        ++write_failures_;
        return false;
    }

    auto* file = std::fopen(path_, "ab");
    if (file == nullptr) {
        ++write_failures_;
        persistent_ = false;
        return false;
    }
    const auto written = std::fwrite(frame.bytes.data(), 1, frame.size, file);
    // Flushed before the handle goes, so a card pulled a moment later has the record rather
    // than whatever the buffer happened to hold.
    const auto flushed = std::fflush(file) == 0;
    const auto closed = std::fclose(file) == 0;
    if (written != frame.size || !flushed || !closed) {
        ++write_failures_;
        persistent_ = false;
        return false;
    }
    persistent_ = true;
    return true;
}

void FileSummaryStore::close() noexcept
{
    cache_.clear();
    scan_ = {};
    path_ = nullptr;
    persistent_ = false;
    write_failures_ = 0;
}

SummaryReadResult FileSummaryStore::session_count(std::size_t& count) noexcept
{
    return cache_.session_count(count);
}

SummaryReadResult FileSummaryStore::read_summary(const std::size_t history_index,
                                                 SessionSummaryV1& summary) noexcept
{
    return cache_.read_summary(history_index, summary);
}

SummaryReadResult FileSummaryStore::read_lap_page(
    const std::array<char, kSessionIdentifierCapacity>& session_id, const std::size_t offset,
    SummaryLapPage& page) noexcept
{
    return cache_.read_lap_page(session_id, offset, page);
}

bool FileSummaryStore::persistent() const noexcept { return persistent_; }

const SummaryScanReport& FileSummaryStore::scan_report() const noexcept { return scan_; }

std::size_t FileSummaryStore::write_failure_count() const noexcept { return write_failures_; }

}  // namespace track_timer::logger
