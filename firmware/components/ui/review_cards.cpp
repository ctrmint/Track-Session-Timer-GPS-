#include "track_timer/ui/review_cards.hpp"

#include <algorithm>
#include <cstdio>

namespace track_timer::ui {
namespace {

void set_clock(std::array<char, 24>& value, const std::int64_t milliseconds) noexcept
{
    const auto total = std::max<std::int64_t>(0, milliseconds) / 1'000;
    std::snprintf(value.data(), value.size(), "%lld:%02lld",
                  static_cast<long long>(total / 60), static_cast<long long>(total % 60));
}

void set_pair(std::array<char, 24>& value, const float first, const float second) noexcept
{
    std::snprintf(value.data(), value.size(), "%.2f / %.2f", static_cast<double>(first),
                  static_cast<double>(second));
}

void append(ReviewCardList& list, const ReviewMetric metric) noexcept
{
    if (list.count < list.cards.size()) {
        list.cards[list.count].metric = metric;
        ++list.count;
    }
}

const char* completion_text(const logger::SessionCompletionReason reason) noexcept
{
    switch (reason) {
    case logger::SessionCompletionReason::driver_stop:
        return "DRIVER";
    case logger::SessionCompletionReason::pit_entry:
        return "PIT ENTRY";
    case logger::SessionCompletionReason::reset_recovery:
        return "RECOVERED";
    case logger::SessionCompletionReason::none:
        break;
    }
    return "UNKNOWN";
}

}  // namespace

ReviewCardList review_cards(const logger::SessionSummaryV1& summary) noexcept
{
    ReviewCardList list{};

    append(list, ReviewMetric::duration);
    set_clock(list.cards[list.count - 1].value, summary.session_duration_ms);

    append(list, ReviewMetric::overrun);
    set_clock(list.cards[list.count - 1].value, summary.session_overrun_ms);

    append(list, ReviewMetric::peak_total);
    std::snprintf(list.cards[list.count - 1].value.data(),
                  list.cards[list.count - 1].value.size(), "%.2f",
                  static_cast<double>(summary.peaks.total_g));

    append(list, ReviewMetric::longitudinal);
    set_pair(list.cards[list.count - 1].value, summary.peaks.acceleration_g,
             summary.peaks.braking_g);

    append(list, ReviewMetric::lateral);
    set_pair(list.cards[list.count - 1].value, summary.peaks.left_g, summary.peaks.right_g);

    append(list, ReviewMetric::vertical);
    set_pair(list.cards[list.count - 1].value, summary.peaks.up_g, summary.peaks.down_g);

    append(list, ReviewMetric::completion);
    std::snprintf(list.cards[list.count - 1].value.data(),
                  list.cards[list.count - 1].value.size(), "%s",
                  completion_text(summary.completion_reason));

    // Only when there is something to say. A card that always reads "fine" teaches the
    // driver to swipe past the one place a problem would be reported.
    if (summary.integrity == logger::SummaryIntegrity::partial_log ||
        summary.degraded_subsystems != logger::degraded_none) {
        append(list, ReviewMetric::integrity);
        const auto* text = summary.integrity == logger::SummaryIntegrity::partial_log
                               ? "PARTIAL LOG"
                               : "NO GPS";
        std::snprintf(list.cards[list.count - 1].value.data(),
                      list.cards[list.count - 1].value.size(), "%s", text);
    }
    return list;
}

const char* review_metric_caption(const ReviewMetric metric) noexcept
{
    switch (metric) {
    case ReviewMetric::duration:
        return "SESSION TIME";
    case ReviewMetric::overrun:
        return "OVER RUN";
    case ReviewMetric::peak_total:
        return "PEAK G";
    case ReviewMetric::longitudinal:
        return "ACCEL / BRAKE  G";
    case ReviewMetric::lateral:
        return "LEFT / RIGHT  G";
    case ReviewMetric::vertical:
        return "UP / DOWN  G";
    case ReviewMetric::completion:
        return "ENDED BY";
    case ReviewMetric::integrity:
        return "RECORDING";
    }
    return "";
}

std::uint32_t review_metric_rgb(const ReviewMetric metric) noexcept
{
    switch (metric) {
    case ReviewMetric::duration:
        return 0x39B6FF;  // azure, as the timer is elsewhere
    case ReviewMetric::overrun:
        return 0x9A4DFF;  // the overrun's own deep purple
    case ReviewMetric::peak_total:
        return 0xFF3B30;
    case ReviewMetric::longitudinal:
        return 0x2FD16D;
    case ReviewMetric::lateral:
        return 0xFFC02E;
    case ReviewMetric::vertical:
        return 0xFF8A24;
    case ReviewMetric::completion:
        return 0x7C8899;
    case ReviewMetric::integrity:
        return 0xFFC02E;
    }
    return 0xFFFFFF;
}

}  // namespace track_timer::ui
