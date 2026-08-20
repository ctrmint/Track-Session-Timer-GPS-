#pragma once

#include "track_timer/logger/memory_summary_store.hpp"
#include "track_timer/logger/summary_frame.hpp"

#include <cstddef>

namespace track_timer::logger {

// Session summaries kept on the card, so a driver's sessions outlive a power cycle.
//
// The card is not assumed to be there. Every failure - absent, full, pulled mid-write -
// leaves the in-memory history intact and reports itself, because a session that has just
// been driven matters more than the record of it, and losing the timer because a card
// misbehaved would be the worse failure by far.
class FileSummaryStore final : public SessionSummaryProvider {
  public:
    enum class OpenResult : std::uint8_t {
        ready,        // a file was read
        no_file,      // nothing there yet, which is simply a card with no sessions on it
        unreadable,   // a path that cannot be opened, usually no card
    };

    // Reads what is on the card into memory. The cache is the newest kCapacity sessions,
    // so a long-lived card does not have to be held in RAM to be reviewed.
    OpenResult open(const char* path) noexcept;
    // Appends to the card and updates the cache. Returns false when the record could not be
    // written; the cache still holds it, so Review shows the session either way.
    bool append(const SessionSummaryV1& summary) noexcept;
    void close() noexcept;

    [[nodiscard]] SummaryReadResult session_count(std::size_t& count) noexcept override;
    [[nodiscard]] SummaryReadResult read_summary(std::size_t history_index,
                                                 SessionSummaryV1& summary) noexcept override;
    [[nodiscard]] SummaryReadResult read_lap_page(
        const std::array<char, kSessionIdentifierCapacity>& session_id, std::size_t offset,
        SummaryLapPage& page) noexcept override;

    [[nodiscard]] bool persistent() const noexcept;
    [[nodiscard]] const SummaryScanReport& scan_report() const noexcept;
    [[nodiscard]] std::size_t write_failure_count() const noexcept;

  private:
    MemorySummaryStore cache_{};
    const char* path_{nullptr};
    SummaryScanReport scan_{};
    std::size_t write_failures_{0};
    bool persistent_{false};
};

}  // namespace track_timer::logger
