#include "track_timer/simulator/summary_fixtures.hpp"
#include "track_timer/ui/session_review.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

int main()
{
    using namespace track_timer;

    simulator::SummaryFixtureProvider complete{simulator::SummaryFixtureId::complete};
    ui::SessionReviewController review{};
    review.begin(&complete);
    auto view = review.view_model();
    assert(view.status == ui::SessionReviewStatus::ready);
    assert(view.session_count == 2);
    assert(view.history_index == 0);
    assert(view.lap_count == 7);
    assert(view.laps[0].visible);
    assert(view.laps[3].visible);
    assert(!view.previous_page_enabled);
    assert(view.next_page_enabled);
    assert(!view.newer_session_enabled);
    assert(view.older_session_enabled);
    assert(std::strcmp(view.integrity.data(), "DEGRADED: GPS / IMU") == 0);

    review.next_lap_page();
    view = review.view_model();
    assert(view.lap_offset == 4);
    assert(view.previous_page_enabled);
    assert(!view.next_page_enabled);
    assert(view.laps[0].best);
    assert(std::strcmp(view.laps[0].emphasis.data(), "BEST") == 0);
    assert(view.laps[2].previous);
    assert(std::strcmp(view.laps[2].emphasis.data(), "PREVIOUS") == 0);
    review.next_lap_page();
    assert(review.view_model().lap_offset == 4);

    review.previous_lap_page();
    assert(review.view_model().lap_offset == 0);
    review.older_session();
    view = review.view_model();
    assert(view.history_index == 1);
    assert(view.lap_count == 3);
    assert(view.newer_session_enabled);
    assert(!view.older_session_enabled);
    assert(view.laps[1].best);
    assert(view.laps[2].previous);
    review.older_session();
    assert(review.view_model().history_index == 1);
    review.newer_session();
    assert(review.view_model().history_index == 0);

    simulator::SummaryFixtureProvider partial{simulator::SummaryFixtureId::partial};
    review.begin(&partial);
    view = review.view_model();
    assert(view.status == ui::SessionReviewStatus::partial_log);
    assert(std::strstr(view.integrity.data(), "PARTIAL LOG") != nullptr);

    simulator::SummaryFixtureProvider empty{simulator::SummaryFixtureId::empty};
    review.begin(&empty);
    assert(review.view_model().status == ui::SessionReviewStatus::empty);
    assert(review.view_model().duration[0] == '\0');

    simulator::SummaryFixtureProvider missing{simulator::SummaryFixtureId::missing};
    review.begin(&missing);
    assert(review.view_model().status == ui::SessionReviewStatus::storage_unavailable);

    simulator::SummaryFixtureProvider corrupt{simulator::SummaryFixtureId::corrupt};
    review.begin(&corrupt);
    assert(review.view_model().status == ui::SessionReviewStatus::corrupt);

    simulator::SummaryFixtureProvider unsupported{simulator::SummaryFixtureId::unsupported};
    review.begin(&unsupported);
    assert(review.view_model().status == ui::SessionReviewStatus::unsupported_version);

    review.begin(nullptr);
    assert(review.view_model().status == ui::SessionReviewStatus::storage_unavailable);
    review.close();
    assert(review.view_model().status == ui::SessionReviewStatus::closed);

    std::cout << "Bounded session review paging, emphasis, and failure states passed\n";
    return 0;
}
