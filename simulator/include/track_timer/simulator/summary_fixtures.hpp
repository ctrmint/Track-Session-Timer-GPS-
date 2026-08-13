#pragma once

#include "track_timer/logger/summary_provider.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace track_timer::simulator {

enum class SummaryFixtureId : std::uint8_t {
    complete,
    partial,
    empty,
    missing,
    corrupt,
    unsupported,
};

class SummaryFixtureProvider final : public logger::SessionSummaryProvider {
  public:
    explicit SummaryFixtureProvider(SummaryFixtureId fixture) noexcept;

    [[nodiscard]] logger::SummaryReadResult session_count(
        std::size_t& count) noexcept override;
    [[nodiscard]] logger::SummaryReadResult read_summary(
        std::size_t history_index, logger::SessionSummaryV1& summary) noexcept override;
    [[nodiscard]] logger::SummaryReadResult read_lap_page(
        const std::array<char, logger::kSessionIdentifierCapacity>& session_id,
        std::size_t offset, logger::SummaryLapPage& page) noexcept override;

    [[nodiscard]] SummaryFixtureId fixture() const noexcept;

  private:
    static constexpr std::size_t kSessionCapacity = 2;
    static constexpr std::size_t kLapCapacity = 7;

    SummaryFixtureId fixture_{SummaryFixtureId::complete};
    std::array<logger::SessionSummaryV1, kSessionCapacity> summaries_{};
    std::array<std::array<logger::SummaryLapRecordV1, kLapCapacity>, kSessionCapacity> laps_{};
    std::array<std::size_t, kSessionCapacity> lap_counts_{};
};

[[nodiscard]] bool parse_summary_fixture(std::string_view name,
                                         SummaryFixtureId& fixture) noexcept;
[[nodiscard]] const char* summary_fixture_name(SummaryFixtureId fixture) noexcept;

}  // namespace track_timer::simulator
