#pragma once

#include "track_timer/logger/summary_provider.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::ui {

enum class SessionReviewStatus : std::uint8_t {
    closed,
    ready,
    partial_log,
    empty,
    storage_unavailable,
    corrupt,
    unsupported_version,
};

struct SessionReviewLapRow {
    std::array<char, 16> lap{};
    std::array<char, 24> duration{};
    std::array<char, 24> emphasis{};
    bool visible{false};
    bool best{false};
    bool previous{false};
};

struct SessionReviewViewModel {
    SessionReviewStatus status{SessionReviewStatus::closed};
    std::array<char, 32> title{};
    std::array<char, 40> duration{};
    std::array<char, 40> overrun{};
    std::array<char, 32> completion{};
    std::array<char, 64> integrity{};
    std::array<char, 96> message{};
    // What the car pulled. Recorded on the summary since the peaks landed there, and shown
    // here because a session without lap times still has this much to say about itself.
    std::array<char, 16> peak_total{};
    std::array<char, 32> peak_longitudinal{};
    std::array<char, 32> peak_lateral{};
    std::array<char, 32> peak_vertical{};
    std::array<SessionReviewLapRow, logger::kSummaryLapPageCapacity> laps{};
    bool newer_session_enabled{false};
    bool older_session_enabled{false};
    bool previous_page_enabled{false};
    bool next_page_enabled{false};
    std::size_t history_index{0};
    std::size_t session_count{0};
    std::size_t lap_offset{0};
    std::size_t lap_count{0};
};

class SessionReviewController {
  public:
    void begin(logger::SessionSummaryProvider* provider) noexcept;
    void close() noexcept;
    void newer_session() noexcept;
    void older_session() noexcept;
    void previous_lap_page() noexcept;
    void next_lap_page() noexcept;

    [[nodiscard]] const SessionReviewViewModel& view_model() const noexcept;

  private:
    void set_failure(logger::SummaryReadResult result) noexcept;
    void load_session() noexcept;
    void load_laps(std::size_t offset) noexcept;
    void update_view() noexcept;

    logger::SessionSummaryProvider* provider_{nullptr};
    logger::SessionSummaryV1 summary_{};
    logger::SummaryLapPage page_{};
    SessionReviewViewModel view_{};
};

[[nodiscard]] const char* session_review_status_name(SessionReviewStatus status) noexcept;

}  // namespace track_timer::ui
