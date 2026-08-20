#include "track_timer/logger/memory_summary_store.hpp"

#include <algorithm>

namespace track_timer::logger {

bool MemorySummaryStore::record(const SessionSummaryV1& summary) noexcept
{
    if (!valid_summary(summary)) {
        ++rejected_;
        return false;
    }
    // Newest first, so history_index 0 is the session that just finished. The oldest falls
    // off the end rather than the newest being refused: a driver wants the last session far
    // more than the eighth one back.
    const auto keep = std::min(count_, kCapacity - 1);
    for (std::size_t index = keep; index > 0; --index) {
        summaries_[index] = summaries_[index - 1];
    }
    summaries_[0] = summary;
    count_ = std::min(count_ + 1, kCapacity);
    return true;
}

void MemorySummaryStore::clear() noexcept
{
    summaries_.fill({});
    count_ = 0;
    rejected_ = 0;
}

SummaryReadResult MemorySummaryStore::session_count(std::size_t& count) noexcept
{
    count = count_;
    return count_ == 0 ? SummaryReadResult::empty : SummaryReadResult::ready;
}

SummaryReadResult MemorySummaryStore::read_summary(const std::size_t history_index,
                                                   SessionSummaryV1& summary) noexcept
{
    if (count_ == 0) {
        return SummaryReadResult::empty;
    }
    if (history_index >= count_) {
        return SummaryReadResult::corrupt;
    }
    summary = summaries_[history_index];
    return SummaryReadResult::ready;
}

SummaryReadResult MemorySummaryStore::read_lap_page(
    const std::array<char, kSessionIdentifierCapacity>& session_id, const std::size_t offset,
    SummaryLapPage& page) noexcept
{
    (void)session_id;
    page = {};
    page.offset = offset;
    return SummaryReadResult::ready;
}

std::size_t MemorySummaryStore::rejected_count() const noexcept { return rejected_; }

}  // namespace track_timer::logger
