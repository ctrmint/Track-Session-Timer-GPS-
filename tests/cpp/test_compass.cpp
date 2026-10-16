#include "track_timer/ui/compass.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

namespace {

using namespace track_timer;

// A car travelling east at 30 m/s with the receiver confident about the course.
ui::GpsOnlySnapshot driving(const float heading_deg = 90.0F)
{
    ui::GpsOnlySnapshot snapshot{};
    snapshot.receiver_present = true;
    snapshot.fix_valid = true;
    snapshot.fix.fix_type = domain::FixType::fix_3d;
    snapshot.fix.num_satellites = 14;
    snapshot.fix.speed_mps = 30.0F;
    snapshot.fix.heading_deg = heading_deg;
    snapshot.fix.heading_accuracy_deg = 1.5F;
    return snapshot;
}

void an_absent_receiver_points_nowhere()
{
    const auto view = ui::compass_view({});
    assert(view.state == ui::CompassState::no_receiver);
    assert(!view.heading_valid);
    assert(std::strcmp(view.degrees.data(), "---") == 0);
    assert(view.cardinal[0] == '\0');
    assert(std::strcmp(view.status.data(), "NO RECEIVER") == 0);
}

void a_searching_receiver_is_not_an_absent_one()
{
    ui::GpsOnlySnapshot snapshot{};
    snapshot.receiver_present = true;
    const auto view = ui::compass_view(snapshot);
    assert(view.state == ui::CompassState::no_fix);
    assert(!view.heading_valid);
    assert(std::strcmp(view.status.data(), "NO FIX") == 0);
}

// The state this mode exists for: parked with a perfect fix, knowing exactly where it is
// and nothing whatever about which way it faces.
void a_parked_car_has_no_heading_however_good_the_fix()
{
    auto snapshot = driving();
    snapshot.fix.speed_mps = 0.0F;
    snapshot.fix.num_satellites = 16;
    const auto view = ui::compass_view(snapshot);
    assert(view.state == ui::CompassState::no_heading);
    // No needle, and no number either. A frozen compass looks exactly like a right one,
    // which is why neither may survive the car stopping.
    assert(!view.heading_valid);
    assert(std::strcmp(view.degrees.data(), "---") == 0);
    assert(view.cardinal[0] == '\0');
    // Named for the cause. The fix is fine; saying NO FIX here would send somebody to the
    // wrong problem entirely.
    assert(std::strcmp(view.status.data(), "NO HEADING") == 0);
}

void a_receiver_unsure_of_the_course_is_refused()
{
    auto snapshot = driving();
    snapshot.fix.heading_accuracy_deg = ui::kMaximumHeadingAccuracyDeg + 0.1F;
    assert(ui::compass_state(snapshot) == ui::CompassState::no_heading);

    snapshot.fix.heading_accuracy_deg = ui::kMaximumHeadingAccuracyDeg;
    assert(ui::compass_state(snapshot) == ui::CompassState::live);

    // A receiver reporting perfect confidence is reporting nothing: headAcc is an
    // uncertainty, and zero is what an unpopulated field looks like.
    snapshot.fix.heading_accuracy_deg = 0.0F;
    assert(ui::compass_state(snapshot) == ui::CompassState::no_heading);
}

void the_speed_floor_backs_up_the_accuracy_gate()
{
    auto snapshot = driving();
    snapshot.fix.speed_mps = ui::kMinimumHeadingSpeedMps;
    assert(ui::compass_state(snapshot) == ui::CompassState::live);
    snapshot.fix.speed_mps = ui::kMinimumHeadingSpeedMps - 0.01F;
    assert(ui::compass_state(snapshot) == ui::CompassState::no_heading);
}

void a_heading_renders_zero_padded_to_three_digits()
{
    assert(std::strcmp(ui::compass_view(driving(90.0F)).degrees.data(), "090") == 0);
    assert(std::strcmp(ui::compass_view(driving(7.0F)).degrees.data(), "007") == 0);
    assert(std::strcmp(ui::compass_view(driving(359.0F)).degrees.data(), "359") == 0);
    // Three digits always, which is what spares this mode the repositioning Speed Only
    // needs when its digit count changes.
    for (float bearing = 0.0F; bearing < 360.0F; bearing += 1.0F) {
        assert(std::strlen(ui::compass_view(driving(bearing)).degrees.data()) == 3);
    }
}

void north_wraps_rather_than_reading_three_hundred_and_sixty()
{
    assert(std::strcmp(ui::compass_view(driving(360.0F)).degrees.data(), "000") == 0);
    assert(std::strcmp(ui::compass_view(driving(359.7F)).degrees.data(), "000") == 0);
    // A bearing arriving outside the turn must wrap, not index out of the cardinal table
    // or spill a fourth digit into a three-digit field.
    assert(std::strcmp(ui::compass_view(driving(-90.0F)).degrees.data(), "270") == 0);
    assert(std::strcmp(ui::compass_view(driving(450.0F)).degrees.data(), "090") == 0);
}

void every_cardinal_owns_the_sector_centred_on_itself()
{
    // Each point is centred on its own bearing, so east runs 78.75 to 101.25 - not 90 to
    // 112.5. Truncating instead of rounding shifts every label by half a sector, which
    // reads as correct until somebody checks it against a map.
    assert(std::strcmp(ui::compass_cardinal(0.0F), "N") == 0);
    assert(std::strcmp(ui::compass_cardinal(90.0F), "E") == 0);
    assert(std::strcmp(ui::compass_cardinal(180.0F), "S") == 0);
    assert(std::strcmp(ui::compass_cardinal(270.0F), "W") == 0);
    assert(std::strcmp(ui::compass_cardinal(45.0F), "NE") == 0);
    assert(std::strcmp(ui::compass_cardinal(22.5F), "NNE") == 0);

    // The boundaries either side of north, which is the wrap the table has to survive.
    assert(std::strcmp(ui::compass_cardinal(11.2F), "N") == 0);
    assert(std::strcmp(ui::compass_cardinal(11.3F), "NNE") == 0);
    assert(std::strcmp(ui::compass_cardinal(348.8F), "N") == 0);
    assert(std::strcmp(ui::compass_cardinal(348.7F), "NNW") == 0);

    // Every bearing in the turn maps to one of the sixteen, and never off the end.
    for (float bearing = 0.0F; bearing < 360.0F; bearing += 0.5F) {
        const auto* point = ui::compass_cardinal(bearing);
        assert(point != nullptr);
        const auto length = std::strlen(point);
        assert(length >= 1 && length <= 3);
    }
}

void the_marker_bearing_matches_the_number()
{
    const auto view = ui::compass_view(driving(123.4F));
    assert(view.heading_valid);
    assert(std::fabs(view.heading_deg - 123.4F) < 0.01F);
    assert(std::strcmp(view.degrees.data(), "123") == 0);
    assert(std::strcmp(view.cardinal.data(), "ESE") == 0);
}

void every_state_has_a_name_a_colour_and_untruncated_text()
{
    ui::GpsOnlySnapshot searching{};
    searching.receiver_present = true;
    auto parked = driving();
    parked.fix.speed_mps = 0.0F;

    const ui::CompassViewModel views[] = {
        ui::compass_view({}), ui::compass_view(searching), ui::compass_view(parked),
        ui::compass_view(driving()),
    };
    for (const auto& view : views) {
        assert(std::strlen(view.status.data()) < ui::kCompassStatusCapacity - 1);
        assert(view.status_rgb != 0);
        assert(std::strlen(ui::compass_state_name(view.state)) > 0);
    }
    assert(views[0].status_rgb != views[3].status_rgb);
    // Only the last one may draw anything.
    assert(!views[0].heading_valid && !views[1].heading_valid && !views[2].heading_valid);
    assert(views[3].heading_valid);
}

}  // namespace

int main()
{
    an_absent_receiver_points_nowhere();
    a_searching_receiver_is_not_an_absent_one();
    a_parked_car_has_no_heading_however_good_the_fix();
    a_receiver_unsure_of_the_course_is_refused();
    the_speed_floor_backs_up_the_accuracy_gate();
    a_heading_renders_zero_padded_to_three_digits();
    north_wraps_rather_than_reading_three_hundred_and_sixty();
    every_cardinal_owns_the_sector_centred_on_itself();
    the_marker_bearing_matches_the_number();
    every_state_has_a_name_a_colour_and_untruncated_text();

    std::cout << "Compass view model: a parked car reports no heading however good its "
                 "fix, every bearing maps to its own cardinal sector, and north wraps\n";
    return 0;
}
