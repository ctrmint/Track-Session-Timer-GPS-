#pragma once

#include "track_timer/logger/summary_provider.hpp"

#include <array>
#include <cstddef>

namespace track_timer::logger {

// Holds the last few session summaries in RAM so Review has something to show the moment a
// session ends.
//
// Deliberately not the durable answer: nothing here survives a power cycle, and the card is
// where these belong. It exists so the record's shape is settled and visible before the
// write path is built on top of it, and so Review stops being wired to nullptr.
class MemorySummaryStore final : public SessionSummaryProvider {
  public:
    static constexpr std::size_t kCapacity = 8;

    // Newest first. Returns false if the summary would not survive its own validator, so a
    // malformed record cannot reach Review and be rendered as though it were real.
    bool record(const SessionSummaryV1& summary) noexcept;
    void clear() noexcept;

    [[nodiscard]] SummaryReadResult session_count(std::size_t& count) noexcept override;
    [[nodiscard]] SummaryReadResult read_summary(std::size_t history_index,
                                                 SessionSummaryV1& summary) noexcept override;
    // No laps without a receiver. Reported as an empty page rather than an error, because
    // a session genuinely having no laps is not a fault.
    [[nodiscard]] SummaryReadResult read_lap_page(
        const std::array<char, kSessionIdentifierCapacity>& session_id, std::size_t offset,
        SummaryLapPage& page) noexcept override;

    [[nodiscard]] std::size_t rejected_count() const noexcept;

  private:
    std::array<SessionSummaryV1, kCapacity> summaries_{};
    std::size_t count_{0};
    std::size_t rejected_{0};
};

}  // namespace track_timer::logger
