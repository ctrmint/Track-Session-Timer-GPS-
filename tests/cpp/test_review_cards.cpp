#include "track_timer/ui/review_cards.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>

namespace {

using namespace track_timer;

logger::SessionSummaryV1 driven_session()
{
    logger::SessionSummaryV1 summary{};
    summary.record_size_bytes = static_cast<std::uint16_t>(sizeof(summary));
    std::snprintf(summary.session_id.data(), summary.session_id.size(), "S001");
    summary.session_duration_ms = 22 * 60'000 + 34'000;
    summary.session_overrun_ms = 2 * 60'000 + 34'000;
    summary.completion_reason = logger::SessionCompletionReason::driver_stop;
    summary.integrity = logger::SummaryIntegrity::complete;
    summary.peaks.acceleration_g = 0.62F;
    summary.peaks.braking_g = 1.14F;
    summary.peaks.left_g = 0.98F;
    summary.peaks.right_g = 1.02F;
    summary.peaks.up_g = 0.44F;
    summary.peaks.down_g = 0.71F;
    summary.peaks.total_g = 1.21F;
    return summary;
}

[[nodiscard]] const ui::ReviewCard* find(const ui::ReviewCardList& list,
                                         const ui::ReviewMetric metric)
{
    for (std::size_t index = 0; index < list.count; ++index) {
        if (list.cards[index].metric == metric) {
            return &list.cards[index];
        }
    }
    return nullptr;
}

// Every value the record holds gets a card, and reads as a value rather than a label.
void every_recorded_value_gets_a_card()
{
    const auto list = ui::review_cards(driven_session());
    assert(list.count >= 7);
    assert(list.count <= ui::kReviewCardCapacity);

    assert(std::strcmp(find(list, ui::ReviewMetric::duration)->value.data(), "22:34") == 0);
    assert(std::strcmp(find(list, ui::ReviewMetric::overrun)->value.data(), "2:34") == 0);
    assert(std::strcmp(find(list, ui::ReviewMetric::peak_total)->value.data(), "1.21") == 0);
    // Two sides on one card: braking is read against acceleration, not in isolation.
    assert(std::strcmp(find(list, ui::ReviewMetric::longitudinal)->value.data(),
                       "0.62 / 1.14") == 0);
    assert(std::strcmp(find(list, ui::ReviewMetric::lateral)->value.data(),
                       "0.98 / 1.02") == 0);
    assert(std::strcmp(find(list, ui::ReviewMetric::vertical)->value.data(),
                       "0.44 / 0.71") == 0);
    assert(std::strcmp(find(list, ui::ReviewMetric::completion)->value.data(), "DRIVER") == 0);
}

// Every card has to say what it is, or a number on its own means nothing.
void every_card_is_named_and_coloured()
{
    const auto list = ui::review_cards(driven_session());
    for (std::size_t index = 0; index < list.count; ++index) {
        const auto metric = list.cards[index].metric;
        assert(std::strlen(ui::review_metric_caption(metric)) > 0);
        assert(std::strlen(list.cards[index].value.data()) > 0);
        assert(ui::review_metric_rgb(metric) != 0);
    }
}

// A card that always reads "fine" teaches the driver to swipe past the one place a problem
// would be reported, so it only appears when there is something to report.
void the_recording_card_appears_only_when_it_has_something_to_say()
{
    auto clean = driven_session();
    clean.integrity = logger::SummaryIntegrity::complete;
    clean.degraded_subsystems = logger::degraded_none;
    assert(find(ui::review_cards(clean), ui::ReviewMetric::integrity) == nullptr);

    auto without_gnss = driven_session();
    without_gnss.degraded_subsystems = logger::degraded_gnss;
    const auto* card = find(ui::review_cards(without_gnss), ui::ReviewMetric::integrity);
    assert(card != nullptr);
    assert(std::strcmp(card->value.data(), "NO GPS") == 0);

    auto partial = driven_session();
    partial.integrity = logger::SummaryIntegrity::partial_log;
    card = find(ui::review_cards(partial), ui::ReviewMetric::integrity);
    assert(card != nullptr);
    assert(std::strcmp(card->value.data(), "PARTIAL LOG") == 0);
}

// A session with nothing in it still has to produce a coherent set of cards rather than
// blanks the driver has to interpret.
void an_empty_session_still_reads_as_zeroes()
{
    logger::SessionSummaryV1 empty{};
    empty.completion_reason = logger::SessionCompletionReason::driver_stop;
    const auto list = ui::review_cards(empty);
    assert(list.count >= 7);
    assert(std::strcmp(find(list, ui::ReviewMetric::duration)->value.data(), "0:00") == 0);
    assert(std::strcmp(find(list, ui::ReviewMetric::peak_total)->value.data(), "0.00") == 0);
    assert(std::strcmp(find(list, ui::ReviewMetric::vertical)->value.data(),
                       "0.00 / 0.00") == 0);
}

// A session over an hour is minutes and seconds like everything else, not a truncated clock.
void a_long_session_keeps_counting_in_minutes()
{
    auto endurance = driven_session();
    endurance.session_duration_ms = 95 * 60'000 + 7'000;
    endurance.session_overrun_ms = 0;
    const auto list = ui::review_cards(endurance);
    assert(std::strcmp(find(list, ui::ReviewMetric::duration)->value.data(), "95:07") == 0);
}

}  // namespace

int main()
{
    every_recorded_value_gets_a_card();
    every_card_is_named_and_coloured();
    the_recording_card_appears_only_when_it_has_something_to_say();
    an_empty_session_still_reads_as_zeroes();
    a_long_session_keeps_counting_in_minutes();

    std::cout << "Review cards: one value per card, named and coloured, with the recording "
                 "card shown only when it has something to report passed\n";
    return 0;
}
