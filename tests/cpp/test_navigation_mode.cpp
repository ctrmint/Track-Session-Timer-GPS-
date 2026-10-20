#include "track_timer/ui/navigation_mode.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

namespace {

using namespace track_timer;

constexpr std::int64_t kSecond = 1'000'000;

ui::NavigationSnapshot nothing()
{
    return ui::NavigationSnapshot{};
}

// A receiver that is present, fixed, and moving at the given speed and course.
ui::NavigationSnapshot driving(const float speed_mps, const float heading_deg)
{
    ui::NavigationSnapshot snapshot{};
    snapshot.gnss.receiver_present = true;
    snapshot.gnss.fix_valid = true;
    snapshot.gnss.fix.fix_type = domain::FixType::fix_3d;
    snapshot.gnss.fix.speed_mps = speed_mps;
    snapshot.gnss.fix.heading_deg = heading_deg;
    snapshot.gnss.fix.heading_accuracy_deg = 2.0F;
    snapshot.gnss.fix.accepted_for_timing = true;

    snapshot.clock.receiver_present = true;
    snapshot.clock.utc.valid = true;
    snapshot.clock.utc.year = 2026;
    snapshot.clock.utc.month = 10;
    snapshot.clock.utc.day = 20;
    snapshot.clock.utc.hour = 14;
    snapshot.clock.utc.minute = 32;
    snapshot.clock.utc.second = 7;
    snapshot.clock.utc_monotonic_us = 10 * kSecond;
    snapshot.clock.now_monotonic_us = 10 * kSecond;
    return snapshot;
}

// The mode composes three view builders rather than formatting anything itself. That is
// the whole design, so it is the first thing asserted: each readout must arrive with the
// value its own mode would have shown for the same snapshot.
void each_readout_matches_the_mode_it_came_from()
{
    const auto snapshot = driving(31.3F, 90.0F);  // about 70 mph, due east
    const auto model = ui::navigation_view(snapshot);

    assert(std::strcmp(model.speed.speed.data(),
                       ui::speed_only_view(snapshot.gnss).speed.data()) == 0);
    assert(std::strcmp(model.time.time.data(),
                       ui::clock_view(snapshot.clock).time.data()) == 0);
    assert(std::strcmp(model.heading.degrees.data(),
                       ui::compass_view(snapshot.gnss).degrees.data()) == 0);

    // And the values themselves, so a composition that silently returned defaults would
    // not pass on the strength of agreeing with itself.
    assert(model.speed.speed_valid);
    assert(std::strcmp(model.speed.speed.data(), "70") == 0);
    assert(std::strcmp(model.speed.status.data(), "MPH") == 0);
    assert(model.time.digits_valid);
    assert(std::strcmp(model.time.time.data(), "14:32") == 0);
    assert(std::strcmp(model.time.seconds.data(), "07") == 0);
    assert(model.heading.heading_valid);
    assert(std::strcmp(model.heading.degrees.data(), "090") == 0);
    assert(std::strcmp(model.heading.cardinal.data(), "E") == 0);
    assert(!model.has_shared_fault);
}

// The case the mode exists to get right, and the one a combined screen is most likely to
// get wrong. Parked with a good fix, the device knows the speed (zero, truthfully) and the
// time, and knows nothing whatever about which way it is facing: there is no magnetometer
// on this board. The arrow must go, and the other two must stay.
void a_parked_car_keeps_its_speed_and_time_and_loses_the_arrow()
{
    auto snapshot = driving(0.0F, 0.0F);
    // What a receiver standing still actually reports: a course it cannot vouch for.
    snapshot.gnss.fix.heading_accuracy_deg = 80.0F;
    const auto model = ui::navigation_view(snapshot);

    assert(model.speed.speed_valid);
    assert(std::strcmp(model.speed.speed.data(), "0") == 0);
    assert(model.time.digits_valid);
    assert(!model.heading.heading_valid);
    assert(model.heading.state == ui::CompassState::no_heading);
    assert(std::strlen(model.heading.status.data()) > 0);  // says why
    // Not collapsed into one line: three readouts that disagree have three things to say,
    // and the heading is the only one of them the driver cannot work out for themselves.
    assert(!model.has_shared_fault);
}

// An optimistic headAcc while stationary must not get an arrow through on a technicality.
void a_believable_heading_accuracy_does_not_beat_standing_still()
{
    auto snapshot = driving(0.1F, 45.0F);
    snapshot.gnss.fix.heading_accuracy_deg = 1.0F;  // the receiver's own optimism
    const auto model = ui::navigation_view(snapshot);
    assert(!model.heading.heading_valid);
    assert(model.speed.speed_valid);  // 0 mph is a true reading and stays
}

// With no bytes ever received, all three readouts are absent for one reason. Saying it in
// three places reads as three faults, so the mode says it once.
void one_cause_is_stated_once()
{
    const auto model = ui::navigation_view(nothing());

    assert(!model.speed.speed_valid);
    assert(!model.time.digits_valid);
    assert(!model.heading.heading_valid);
    assert(model.has_shared_fault);
    assert(std::strlen(model.shared_fault.data()) > 0);
    // The shared line is one of the readouts' own words, not a fourth wording of the same
    // fault invented here.
    assert(std::strcmp(model.shared_fault.data(), model.speed.status.data()) == 0);
    // And the readouts still carry their dashes, so the screen has something to draw in
    // the cells rather than emptying them.
    assert(std::strcmp(model.speed.speed.data(), "---") == 0);
    assert(std::strcmp(model.time.time.data(), "--:--") == 0);
    assert(std::strcmp(model.heading.degrees.data(), "---") == 0);
}

// A receiver that is talking but has not resolved anything yet is not the shared-fault
// case: it is a different diagnosis from a receiver that is not there, and the two must
// not be shown the same way.
void a_present_receiver_with_nothing_resolved_is_not_the_shared_fault()
{
    ui::NavigationSnapshot snapshot{};
    snapshot.gnss.receiver_present = true;
    snapshot.clock.receiver_present = true;
    const auto model = ui::navigation_view(snapshot);

    assert(!model.speed.speed_valid);
    assert(!model.time.digits_valid);
    assert(!model.heading.heading_valid);
    assert(!model.has_shared_fault);
    // Each says its own thing, and none of them says the receiver is missing.
    assert(std::strlen(model.speed.status.data()) > 0);
    assert(std::strlen(model.time.status.data()) > 0);
    assert(std::strlen(model.heading.status.data()) > 0);
}

// A three-digit speed is the case the screen's layout is sized against, so it is worth
// knowing the view model really does produce one.
void a_three_digit_speed_is_three_characters()
{
    const auto model = ui::navigation_view(driving(53.6F, 180.0F));  // about 120 mph
    assert(model.speed.speed_valid);
    assert(std::strlen(model.speed.speed.data()) == 3);
    assert(std::strcmp(model.heading.cardinal.data(), "S") == 0);
}

// The clock outliving the fix that carried it is why the snapshot carries two receiver
// flags rather than one. A receiver that has gone quiet holds the time and loses the rest.
void a_quiet_receiver_holds_the_clock_and_drops_the_arrow()
{
    auto snapshot = driving(31.3F, 90.0F);
    snapshot.gnss.fix_valid = false;
    snapshot.gnss.fix = {};
    snapshot.clock.now_monotonic_us = snapshot.clock.utc_monotonic_us + 30 * kSecond;
    const auto model = ui::navigation_view(snapshot);

    assert(model.time.digits_valid);
    assert(model.time.state == ui::ClockState::holding);
    assert(!model.speed.speed_valid);
    assert(!model.heading.heading_valid);
    assert(!model.has_shared_fault);
}

}  // namespace

int main()
{
    each_readout_matches_the_mode_it_came_from();
    a_parked_car_keeps_its_speed_and_time_and_loses_the_arrow();
    a_believable_heading_accuracy_does_not_beat_standing_still();
    one_cause_is_stated_once();
    a_present_receiver_with_nothing_resolved_is_not_the_shared_fault();
    a_three_digit_speed_is_three_characters();
    a_quiet_receiver_holds_the_clock_and_drops_the_arrow();

    std::cout << "Navigation mode: composed readouts, a parked car losing only its arrow, "
                 "and one cause stated once passed\n";
    return 0;
}
