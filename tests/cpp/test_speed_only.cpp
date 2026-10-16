#include "track_timer/ui/speed_only.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

namespace {

using namespace track_timer;

// The same car the GPS Only and UBX parser fixtures use - 62.6 m/s, about 140 mph - so the
// three agree on what a fast lap looks like.
ui::GpsOnlySnapshot moving()
{
    ui::GpsOnlySnapshot snapshot{};
    snapshot.receiver_present = true;
    snapshot.fix_valid = true;
    snapshot.fix.fix_type = domain::FixType::fix_3d;
    snapshot.fix.num_satellites = 14;
    snapshot.fix.speed_mps = 62.6F;
    return snapshot;
}

void an_absent_receiver_shows_dashes_rather_than_zero()
{
    const auto view = ui::speed_only_view({});
    assert(view.state == ui::GpsOnlyState::no_receiver);
    assert(!view.speed_valid);
    // The whole reason this mode needs care: a stationary car and a dead receiver both
    // have a speed of zero, and 0 is a perfectly believable reading.
    assert(std::strcmp(view.speed.data(), "---") == 0);
    assert(std::strcmp(view.status.data(), "NO RECEIVER") == 0);
}

void a_searching_receiver_is_not_an_absent_one()
{
    ui::GpsOnlySnapshot snapshot{};
    snapshot.receiver_present = true;
    const auto view = ui::speed_only_view(snapshot);
    assert(view.state == ui::GpsOnlyState::no_fix);
    assert(!view.speed_valid);
    assert(std::strcmp(view.speed.data(), "---") == 0);
    // Different text because it is a different problem: one is a cable, the other patience.
    assert(std::strcmp(view.status.data(), "NO FIX") == 0);
    assert(view.status_rgb != ui::speed_only_view({}).status_rgb);
}

void a_fix_reports_road_speed_in_mph()
{
    const auto view = ui::speed_only_view(moving());
    assert(view.state == ui::GpsOnlyState::fixed);
    assert(view.speed_valid);
    assert(std::strcmp(view.speed.data(), "140") == 0);
    assert(std::strcmp(view.status.data(), "MPH") == 0);
}

void a_stationary_car_with_a_fix_reads_zero_not_a_placeholder()
{
    auto snapshot = moving();
    snapshot.fix.speed_mps = 0.0F;
    const auto view = ui::speed_only_view(snapshot);
    // The case that is easy to get backwards. A car standing still really is doing 0 mph,
    // and saying "---" here would report a working receiver as a broken one.
    assert(view.speed_valid);
    assert(std::strcmp(view.speed.data(), "0") == 0);
    assert(std::strcmp(view.status.data(), "MPH") == 0);
}

void the_whole_range_the_panel_is_laid_out_for_renders()
{
    struct Case {
        float mps;
        const char* expected;
    };
    // One, two and three digits, which is the range the three fixed cells exist for.
    const Case cases[] = {
        {0.0F, "0"}, {4.4704F, "10"}, {31.3F, "70"}, {44.704F, "100"}, {67.056F, "150"},
    };
    for (const auto& item : cases) {
        auto snapshot = moving();
        snapshot.fix.speed_mps = item.mps;
        assert(std::strcmp(ui::speed_only_view(snapshot).speed.data(), item.expected) == 0);
    }
}

void rounding_is_to_the_nearest_mile_per_hour()
{
    auto snapshot = moving();
    // 29.9 mph and 30.1 mph either side of the boundary. A truncating conversion would
    // show both as 29 and 30, which reads as a display that sticks.
    snapshot.fix.speed_mps = 29.9F / ui::kMetresPerSecondToMph;
    assert(std::strcmp(ui::speed_only_view(snapshot).speed.data(), "30") == 0);
    snapshot.fix.speed_mps = 29.4F / ui::kMetresPerSecondToMph;
    assert(std::strcmp(ui::speed_only_view(snapshot).speed.data(), "29") == 0);
}

void an_implausible_reading_cannot_overrun_the_field()
{
    auto snapshot = moving();
    // snprintf truncates silently, so a four-digit number would leave a plausible
    // three-digit one behind - a receiver fault rendered as a believable speed.
    snapshot.fix.speed_mps = 10'000.0F;
    const auto view = ui::speed_only_view(snapshot);
    assert(std::strlen(view.speed.data()) < ui::kSpeedOnlyTextCapacity);
    assert(std::strcmp(view.speed.data(), "999") == 0);

    // A negative speed is not a thing a receiver should report, and must not render as a
    // minus sign eating a digit cell.
    snapshot.fix.speed_mps = -5.0F;
    assert(std::strcmp(ui::speed_only_view(snapshot).speed.data(), "0") == 0);
}

void every_state_has_its_own_colour_and_no_text_is_truncated()
{
    ui::GpsOnlySnapshot searching{};
    searching.receiver_present = true;
    const ui::SpeedOnlyViewModel views[] = {
        ui::speed_only_view({}), ui::speed_only_view(searching), ui::speed_only_view(moving()),
    };
    for (const auto& view : views) {
        assert(std::strlen(view.status.data()) < ui::kSpeedOnlyStatusCapacity - 1);
        assert(view.status_rgb != 0);
    }
    assert(views[0].status_rgb != views[1].status_rgb);
    assert(views[1].status_rgb != views[2].status_rgb);
}

}  // namespace

int main()
{
    an_absent_receiver_shows_dashes_rather_than_zero();
    a_searching_receiver_is_not_an_absent_one();
    a_fix_reports_road_speed_in_mph();
    a_stationary_car_with_a_fix_reads_zero_not_a_placeholder();
    the_whole_range_the_panel_is_laid_out_for_renders();
    rounding_is_to_the_nearest_mile_per_hour();
    an_implausible_reading_cannot_overrun_the_field();
    every_state_has_its_own_colour_and_no_text_is_truncated();

    std::cout << "Speed Only view model: a stationary car reads zero, an absent or "
                 "unfixed receiver never does, and one, two and three digit speeds all "
                 "render\n";
    return 0;
}
