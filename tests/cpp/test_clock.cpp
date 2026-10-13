#include "track_timer/ui/clock.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

namespace {

using namespace track_timer;

// A real instant, chosen so every field is distinguishable: 2026-10-13 14:32:07 UTC.
// Nothing here is zero, so a field that is dropped or read from the wrong offset shows up
// as a wrong digit rather than a plausible one.
domain::UtcTime sample_time()
{
    domain::UtcTime utc{};
    utc.year = 2026;
    utc.month = 10;
    utc.day = 13;
    utc.hour = 14;
    utc.minute = 32;
    utc.second = 7;
    utc.nanosecond = 0;
    utc.valid = true;
    return utc;
}

ui::ClockSnapshot live_snapshot()
{
    ui::ClockSnapshot snapshot{};
    snapshot.utc = sample_time();
    snapshot.receiver_present = true;
    snapshot.utc_monotonic_us = 1'000'000;
    snapshot.now_monotonic_us = 1'000'000;
    return snapshot;
}

void an_absent_receiver_shows_dashes_rather_than_midnight()
{
    const ui::ClockSnapshot snapshot{};
    const auto view = ui::clock_view(snapshot);
    assert(view.state == ui::ClockState::no_receiver);
    assert(!view.digits_valid);
    // The whole point of the mode's failure handling: 00:00 is a believable time, so an
    // absent receiver must never be able to produce it.
    assert(std::strcmp(view.time.data(), "--:--") == 0);
    assert(std::strcmp(view.seconds.data(), "--") == 0);
    assert(std::strcmp(view.status.data(), "NO RECEIVER") == 0);
    assert(view.date[0] == '\0');
}

void a_receiver_that_has_not_resolved_the_time_is_not_an_absent_one()
{
    ui::ClockSnapshot snapshot{};
    snapshot.receiver_present = true;
    const auto view = ui::clock_view(snapshot);
    assert(view.state == ui::ClockState::waiting);
    assert(!view.digits_valid);
    assert(std::strcmp(view.time.data(), "--:--") == 0);
    // Different text because it is a different problem: one is a cable, the other patience.
    assert(std::strcmp(view.status.data(), "WAITING FOR TIME") == 0);
}

void a_resolved_time_renders_as_a_24_hour_clock_labelled_gmt()
{
    const auto view = ui::clock_view(live_snapshot());
    assert(view.state == ui::ClockState::live);
    assert(view.digits_valid);
    assert(std::strcmp(view.time.data(), "14:32") == 0);
    assert(std::strcmp(view.seconds.data(), "07") == 0);
    assert(std::strcmp(view.date.data(), "2026-10-13") == 0);
    // Not decoration. GMT is not British Summer Time, so for half the year this reads an
    // hour behind every other clock in the country, and the label is what stops that
    // being mistaken for a fault.
    assert(std::strcmp(view.status.data(), "GMT") == 0);
}

void afternoon_is_never_rendered_as_morning()
{
    auto snapshot = live_snapshot();
    snapshot.utc.hour = 23;
    snapshot.utc.minute = 5;
    const auto view = ui::clock_view(snapshot);
    assert(std::strcmp(view.time.data(), "23:05") == 0);

    snapshot.utc.hour = 0;
    snapshot.utc.minute = 9;
    const auto midnight = ui::clock_view(snapshot);
    // Leading zeros, not spaces: a fixed-width clock whose first digit disappears shifts
    // the whole face.
    assert(std::strcmp(midnight.time.data(), "00:09") == 0);
}

void a_negative_sub_second_borrows_rather_than_rounding_up()
{
    // The receiver reports its calendar fields rounded to the nearest second, with `nano`
    // as the signed correction. 14:32:07 with nano = -400 ms means the true instant is
    // 14:32:06.6 - so showing 07 would be 0.4 s fast, and at 20 Hz roughly half of all
    // readings are this case.
    auto snapshot = live_snapshot();
    snapshot.utc.nanosecond = -400'000'000;
    const auto view = ui::clock_view(snapshot);
    assert(std::strcmp(view.seconds.data(), "06") == 0);
    assert(std::strcmp(view.time.data(), "14:32") == 0);

    // A positive fraction is already inside the named second and must not advance it.
    snapshot.utc.nanosecond = 400'000'000;
    assert(std::strcmp(ui::clock_view(snapshot).seconds.data(), "07") == 0);
}

void the_borrow_carries_through_every_rollover()
{
    auto snapshot = live_snapshot();
    snapshot.utc.nanosecond = -1;
    snapshot.utc.hour = 0;
    snapshot.utc.minute = 0;
    snapshot.utc.second = 0;
    snapshot.utc.year = 2026;
    snapshot.utc.month = 1;
    snapshot.utc.day = 1;
    const auto view = ui::clock_view(snapshot);
    // One nanosecond before the new year: every field has to roll at once, and the year
    // is the one a borrow implemented with per-field carries tends to miss.
    assert(std::strcmp(view.time.data(), "23:59") == 0);
    assert(std::strcmp(view.seconds.data(), "59") == 0);
    assert(std::strcmp(view.date.data(), "2025-12-31") == 0);
}

void the_clock_advances_between_readings()
{
    auto snapshot = live_snapshot();
    // The reading is 900 ms old, which is still fresh but is most of a second: a clock
    // that ignores the age sits on the stale digit until the next poll.
    snapshot.now_monotonic_us = snapshot.utc_monotonic_us + 900'000;
    assert(std::strcmp(ui::clock_view(snapshot).seconds.data(), "07") == 0);

    snapshot.now_monotonic_us = snapshot.utc_monotonic_us + 1'100'000;
    assert(std::strcmp(ui::clock_view(snapshot).seconds.data(), "08") == 0);
}

void a_quiet_receiver_holds_and_says_so_rather_than_freezing()
{
    auto snapshot = live_snapshot();
    snapshot.receiver_present = true;
    snapshot.now_monotonic_us = snapshot.utc_monotonic_us + 125'000'000;  // 125 s
    const auto view = ui::clock_view(snapshot);
    assert(view.state == ui::ClockState::holding);
    // Still showing digits, and they have moved: a frozen clock looks correct and is
    // wrong by however long it has been frozen.
    assert(view.digits_valid);
    assert(std::strcmp(view.time.data(), "14:34") == 0);
    assert(std::strcmp(view.seconds.data(), "12") == 0);
    assert(std::strcmp(view.status.data(), "HOLD 2m") == 0);
    assert(view.status_rgb != ui::clock_view(live_snapshot()).status_rgb);
}

void a_short_hold_is_reported_in_seconds()
{
    auto snapshot = live_snapshot();
    snapshot.now_monotonic_us = snapshot.utc_monotonic_us + 9'000'000;
    const auto view = ui::clock_view(snapshot);
    assert(view.state == ui::ClockState::holding);
    assert(std::strcmp(view.status.data(), "HOLD 9s") == 0);
}

void holding_stops_claiming_once_it_is_no_longer_a_gnss_clock()
{
    auto snapshot = live_snapshot();
    snapshot.now_monotonic_us = snapshot.utc_monotonic_us + ui::kClockHoldLimitUs;
    assert(ui::clock_view(snapshot).state == ui::ClockState::holding);

    snapshot.now_monotonic_us = snapshot.utc_monotonic_us + ui::kClockHoldLimitUs + 1;
    const auto view = ui::clock_view(snapshot);
    assert(view.state == ui::ClockState::lost);
    assert(!view.digits_valid);
    assert(std::strcmp(view.time.data(), "--:--") == 0);
    assert(std::strcmp(view.status.data(), "TIME LOST") == 0);
}

void the_freshness_boundary_is_where_it_is_documented()
{
    auto snapshot = live_snapshot();
    snapshot.now_monotonic_us = snapshot.utc_monotonic_us + ui::kClockFreshnessUs;
    assert(ui::clock_state(snapshot) == ui::ClockState::live);
    snapshot.now_monotonic_us = snapshot.utc_monotonic_us + ui::kClockFreshnessUs + 1;
    assert(ui::clock_state(snapshot) == ui::ClockState::holding);
}

void an_unknown_age_shows_the_reading_as_it_stands()
{
    auto snapshot = live_snapshot();
    snapshot.utc_monotonic_us = domain::kUnavailableTime;
    const auto view = ui::clock_view(snapshot);
    // No monotonic clock means no evidence of staleness, so the receiver's own reading is
    // shown unshifted rather than advanced from a sentinel.
    assert(view.state == ui::ClockState::live);
    assert(std::strcmp(view.seconds.data(), "07") == 0);

    // A monotonic clock that appears to have gone backwards is treated the same way: it
    // must not be allowed to drive the clock backwards too.
    snapshot = live_snapshot();
    snapshot.now_monotonic_us = snapshot.utc_monotonic_us - 5'000'000;
    assert(ui::clock_state(snapshot) == ui::ClockState::live);
    assert(std::strcmp(ui::clock_view(snapshot).seconds.data(), "07") == 0);
}

void a_leap_second_survives_when_nothing_has_to_shift()
{
    auto snapshot = live_snapshot();
    snapshot.utc.month = 12;
    snapshot.utc.day = 31;
    snapshot.utc.hour = 23;
    snapshot.utc.minute = 59;
    snapshot.utc.second = 60;  // a positive leap second, which is a real UTC time
    const auto view = ui::clock_view(snapshot);
    assert(std::strcmp(view.time.data(), "23:59") == 0);
    assert(std::strcmp(view.seconds.data(), "60") == 0);
}

void the_calendar_arithmetic_handles_the_awkward_dates()
{
    const auto base = sample_time();

    // A leap year: 2028-02-28 plus a day is the 29th, not the 1st of March.
    auto utc = base;
    utc.year = 2028;
    utc.month = 2;
    utc.day = 28;
    utc.hour = 23;
    utc.minute = 59;
    utc.second = 59;
    auto shifted = ui::shift_utc(utc, 1);
    assert(shifted.year == 2028 && shifted.month == 2 && shifted.day == 29);
    assert(shifted.hour == 0 && shifted.minute == 0 && shifted.second == 0);

    // A common year: the same instant a year earlier rolls into March.
    utc.year = 2027;
    shifted = ui::shift_utc(utc, 1);
    assert(shifted.year == 2027 && shifted.month == 3 && shifted.day == 1);

    // 2100 is not a leap year, which the simple every-fourth-year rule gets wrong.
    utc.year = 2100;
    shifted = ui::shift_utc(utc, 1);
    assert(shifted.year == 2100 && shifted.month == 3 && shifted.day == 1);

    // Backwards across a month boundary, which is the direction the sub-second borrow uses.
    utc = base;
    utc.month = 3;
    utc.day = 1;
    utc.hour = 0;
    utc.minute = 0;
    utc.second = 0;
    utc.year = 2028;
    shifted = ui::shift_utc(utc, -1);
    assert(shifted.year == 2028 && shifted.month == 2 && shifted.day == 29);
    assert(shifted.hour == 23 && shifted.minute == 59 && shifted.second == 59);
}

void shifting_by_nothing_changes_nothing()
{
    const auto utc = sample_time();
    const auto shifted = ui::shift_utc(utc, 0);
    assert(shifted.year == utc.year && shifted.month == utc.month && shifted.day == utc.day);
    assert(shifted.hour == utc.hour && shifted.minute == utc.minute);
    assert(shifted.second == utc.second && shifted.valid);
}

void a_shift_never_invents_validity()
{
    domain::UtcTime utc{};
    utc.year = 2026;
    utc.month = 10;
    utc.day = 13;
    assert(!ui::shift_utc(utc, 60).valid);
}

void the_consumed_fraction_is_not_left_behind_for_a_second_consumer()
{
    auto utc = sample_time();
    utc.nanosecond = -400'000'000;
    // The fraction has been spent computing the shift. Leaving it in place is how #193
    // happened: two consumers each applying the same correction once.
    assert(ui::shift_utc(utc, -1).nanosecond == 0);
}

void every_state_has_a_name_and_its_own_colour()
{
    const ui::ClockState states[] = {ui::ClockState::no_receiver, ui::ClockState::waiting,
                                     ui::ClockState::live, ui::ClockState::holding,
                                     ui::ClockState::lost};
    for (const auto state : states) {
        assert(std::strlen(ui::clock_state_name(state)) > 0);
    }
    assert(std::strcmp(ui::clock_state_name(ui::ClockState::holding), "holding") == 0);

    ui::ClockSnapshot waiting{};
    waiting.receiver_present = true;
    assert(ui::clock_view(ui::ClockSnapshot{}).status_rgb != ui::clock_view(waiting).status_rgb);
}

void no_status_text_is_truncated()
{
    const ui::ClockState states[] = {ui::ClockState::no_receiver, ui::ClockState::waiting,
                                     ui::ClockState::live, ui::ClockState::holding,
                                     ui::ClockState::lost};
    for (const auto state : states) {
        auto snapshot = live_snapshot();
        switch (state) {
        case ui::ClockState::no_receiver:
            snapshot = ui::ClockSnapshot{};
            break;
        case ui::ClockState::waiting:
            snapshot = ui::ClockSnapshot{};
            snapshot.receiver_present = true;
            break;
        case ui::ClockState::live:
            break;
        case ui::ClockState::holding:
            snapshot.now_monotonic_us = snapshot.utc_monotonic_us + 125'000'000;
            break;
        case ui::ClockState::lost:
            snapshot.now_monotonic_us =
                snapshot.utc_monotonic_us + ui::kClockHoldLimitUs + 1;
            break;
        }
        const auto view = ui::clock_view(snapshot);
        assert(view.state == state);
        // snprintf truncates silently, so the longest string in each state has to be
        // asserted to fit rather than assumed to.
        assert(std::strlen(view.status.data()) < ui::kClockStatusCapacity - 1);
    }
}

}  // namespace

int main()
{
    an_absent_receiver_shows_dashes_rather_than_midnight();
    a_receiver_that_has_not_resolved_the_time_is_not_an_absent_one();
    a_resolved_time_renders_as_a_24_hour_clock_labelled_gmt();
    afternoon_is_never_rendered_as_morning();
    a_negative_sub_second_borrows_rather_than_rounding_up();
    the_borrow_carries_through_every_rollover();
    the_clock_advances_between_readings();
    a_quiet_receiver_holds_and_says_so_rather_than_freezing();
    a_short_hold_is_reported_in_seconds();
    holding_stops_claiming_once_it_is_no_longer_a_gnss_clock();
    the_freshness_boundary_is_where_it_is_documented();
    an_unknown_age_shows_the_reading_as_it_stands();
    a_leap_second_survives_when_nothing_has_to_shift();
    the_calendar_arithmetic_handles_the_awkward_dates();
    shifting_by_nothing_changes_nothing();
    a_shift_never_invents_validity();
    the_consumed_fraction_is_not_left_behind_for_a_second_consumer();
    every_state_has_a_name_and_its_own_colour();
    no_status_text_is_truncated();

    std::cout << "Clock view model: a resolved time renders as 24-hour GMT, an unresolved "
                 "one never renders as midnight, and a quiet receiver holds visibly rather "
                 "than freezing\n";
    return 0;
}
