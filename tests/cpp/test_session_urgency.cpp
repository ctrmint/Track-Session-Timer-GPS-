#include "track_timer/ui/active_session.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

namespace {

using namespace track_timer::ui;
using track_timer::domain::kUnavailableTime;

constexpr std::int64_t minutes(const int value) { return value * 60'000LL; }

bool near(float actual, float expected, float tolerance = 0.001F)
{
    return std::fabs(actual - expected) <= tolerance;
}

// The bar decays from full to empty across the session.
void the_ratio_decays_from_one_to_zero()
{
    const auto total = minutes(20);
    assert(near(session_remaining_ratio(total, total), 1.0F));
    assert(near(session_remaining_ratio(minutes(15), total), 0.75F));
    assert(near(session_remaining_ratio(minutes(10), total), 0.5F));
    assert(near(session_remaining_ratio(0, total), 0.0F));
    // Overtime does not push the bar negative.
    assert(near(session_remaining_ratio(-minutes(3), total), 0.0F));
    // A clock ahead of the nominal duration does not overfill it either.
    assert(near(session_remaining_ratio(minutes(25), total), 1.0F));
}

void an_unknown_or_absent_duration_yields_no_ratio()
{
    assert(near(session_remaining_ratio(minutes(5), 0), 0.0F));
    assert(near(session_remaining_ratio(kUnavailableTime, minutes(20)), 0.0F));
}

// A long session must still walk the whole ramp rather than sitting green for an hour.
void a_long_session_ramps_proportionally()
{
    const auto total = minutes(60);
    assert(session_urgency(minutes(60), total) == SessionUrgency::ample);
    assert(session_urgency(minutes(50), total) == SessionUrgency::ample);
    assert(session_urgency(minutes(40), total) == SessionUrgency::easing);
    assert(session_urgency(minutes(25), total) == SessionUrgency::closing);
    assert(session_urgency(minutes(14), total) == SessionUrgency::urgent);
    // Both signals agree by here: 5 of 60 minutes is 8% left as well as inside the
    // absolute floor, so it is red rather than orange.
    assert(session_urgency(minutes(5), total) == SessionUrgency::critical);
    assert(session_urgency(minutes(2), total) == SessionUrgency::critical);
}

// A short session must not open already amber, which absolute-only thresholds would do:
// a 20 minute session begins with 20 minutes left.
void a_short_session_still_starts_green()
{
    const auto total = minutes(20);
    assert(session_urgency(minutes(20), total) == SessionUrgency::ample);
    assert(session_urgency(minutes(15), total) == SessionUrgency::ample);
    assert(session_urgency(minutes(12), total) == SessionUrgency::easing);
    assert(session_urgency(minutes(8), total) == SessionUrgency::closing);
    assert(session_urgency(minutes(4), total) == SessionUrgency::urgent);
    assert(session_urgency(minutes(1), total) == SessionUrgency::critical);
}

// The absolute floor exists so the closing minutes are urgent whatever the session length.
void the_final_minutes_are_urgent_regardless_of_session_length()
{
    for (const auto total : {minutes(20), minutes(45), minutes(60), minutes(120)}) {
        assert(session_urgency(minutes(2), total) == SessionUrgency::critical);
        assert(session_urgency(minutes(4), total) >= SessionUrgency::urgent);
    }
}

void overtime_is_its_own_band()
{
    assert(session_urgency(0, minutes(20)) == SessionUrgency::overtime);
    assert(session_urgency(-minutes(5), minutes(20)) == SessionUrgency::overtime);
    // Red, like critical: overtime is not a moment to introduce a new colour.
    assert(urgency_rgb(SessionUrgency::overtime) == urgency_rgb(SessionUrgency::critical));
}

// Green through yellow, amber, orange to red, each distinct so the change is noticeable
// at a glance rather than a subtle shift.
void every_band_has_a_distinct_colour()
{
    const SessionUrgency bands[] = {SessionUrgency::ample, SessionUrgency::easing,
                                    SessionUrgency::closing, SessionUrgency::urgent,
                                    SessionUrgency::critical};
    for (std::size_t a = 0; a < 5; ++a) {
        for (std::size_t b = a + 1; b < 5; ++b) {
            assert(urgency_rgb(bands[a]) != urgency_rgb(bands[b]));
        }
    }
    assert(std::strcmp(session_urgency_name(SessionUrgency::closing), "closing") == 0);
}

// Urgency must never go backwards as a session runs down.
void urgency_never_decreases_as_time_runs_out()
{
    const auto total = minutes(45);
    auto previous = SessionUrgency::ample;
    for (std::int64_t remaining = total; remaining > 0; remaining -= 30'000) {
        const auto current = session_urgency(remaining, total);
        assert(static_cast<int>(current) >= static_cast<int>(previous));
        previous = current;
    }
}

}  // namespace

int main()
{
    the_ratio_decays_from_one_to_zero();
    an_unknown_or_absent_duration_yields_no_ratio();
    a_long_session_ramps_proportionally();
    a_short_session_still_starts_green();
    the_final_minutes_are_urgent_regardless_of_session_length();
    overtime_is_its_own_band();
    every_band_has_a_distinct_colour();
    urgency_never_decreases_as_time_runs_out();

    std::cout << "Session countdown ratio and five-band urgency ramp passed\n";
    return 0;
}
