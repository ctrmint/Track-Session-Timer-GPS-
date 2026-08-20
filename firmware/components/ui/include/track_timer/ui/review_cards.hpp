#pragma once

#include "track_timer/logger/formats.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::ui {

// One value per card, cycled with a swipe, in the arrangement the rest of the device uses.
// Free of LVGL so the list a session produces can be tested without a display: what a
// record is worth reading is a question about the record, not about how it is drawn.
enum class ReviewMetric : std::uint8_t {
    duration,
    overrun,
    peak_total,
    longitudinal,
    lateral,
    vertical,
    completion,
    integrity,
};

inline constexpr std::size_t kReviewCardCapacity = 8;

struct ReviewCard {
    ReviewMetric metric{ReviewMetric::duration};
    // The value, large. Two figures where a metric has two sides, because braking and
    // acceleration are read against each other rather than in isolation.
    std::array<char, 24> value{};
};

struct ReviewCardList {
    std::array<ReviewCard, kReviewCardCapacity> cards{};
    std::size_t count{0};
};

[[nodiscard]] ReviewCardList review_cards(const logger::SessionSummaryV1& summary) noexcept;

// The line under the value, saying what it is.
[[nodiscard]] const char* review_metric_caption(ReviewMetric metric) noexcept;
// Colour-coded by what it measures, as the menus are by what they do.
[[nodiscard]] std::uint32_t review_metric_rgb(ReviewMetric metric) noexcept;

}  // namespace track_timer::ui
