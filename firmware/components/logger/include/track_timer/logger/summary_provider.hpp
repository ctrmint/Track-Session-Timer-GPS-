#pragma once

#include "track_timer/logger/formats.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::logger {

inline constexpr std::size_t kSummaryLapPageCapacity = 4;

enum class SummaryReadResult : std::uint8_t {
    ready,
    empty,
    storage_unavailable,
    corrupt,
    unsupported_version,
};

struct SummaryLapPage {
    std::array<SummaryLapRecordV1, kSummaryLapPageCapacity> laps{};
    std::size_t count{0};
    std::size_t offset{0};
    std::size_t total_count{0};
};

// history_index 0 is the newest session. Providers validate transport/framing and
// return versioned records; the consumer still validates semantic fields.
class SessionSummaryProvider {
  public:
    virtual ~SessionSummaryProvider() = default;
    [[nodiscard]] virtual SummaryReadResult session_count(std::size_t& count) noexcept = 0;
    [[nodiscard]] virtual SummaryReadResult read_summary(
        std::size_t history_index, SessionSummaryV1& summary) noexcept = 0;
    [[nodiscard]] virtual SummaryReadResult read_lap_page(
        const std::array<char, kSessionIdentifierCapacity>& session_id,
        std::size_t offset, SummaryLapPage& page) noexcept = 0;
};

}  // namespace track_timer::logger
