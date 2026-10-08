#include "track_timer/ui/gps_only.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

namespace {

using namespace track_timer;

constexpr const char* kNoValue = "---";

// A car at 140 mph, the same sample the UBX parser tests use, so the two agree on what a
// fast lap looks like.
ui::GpsOnlySnapshot fixed_sample()
{
    ui::GpsOnlySnapshot snapshot{};
    snapshot.receiver_present = true;
    snapshot.fix_valid = true;
    snapshot.observed_rate_hz = 25.0F;
    snapshot.dropped_fixes = 0;
    snapshot.fix.fix_type = domain::FixType::fix_3d;
    snapshot.fix.num_satellites = 14;
    snapshot.fix.latitude_deg = 52.4305520;
    snapshot.fix.longitude_deg = -1.4777130;
    snapshot.fix.height_m = 118.0F;
    snapshot.fix.speed_mps = 62.6F;
    snapshot.fix.heading_deg = 274.0F;
    snapshot.fix.horizontal_accuracy_m = 0.8F;
    snapshot.fix.speed_accuracy_mps = 0.12F;
    return snapshot;
}

const ui::GpsOnlyRow& row(const ui::GpsOnlyViewModel& view, const char* const label)
{
    for (const auto& candidate : view.rows) {
        if (std::strcmp(candidate.label.data(), label) == 0) {
            return candidate;
        }
    }
    assert(false && "the view model must carry this row");
    return view.rows[0];
}

bool shows_no_value(const ui::GpsOnlyViewModel& view, const char* const label)
{
    return std::strcmp(row(view, label).value.data(), kNoValue) == 0;
}

// The whole point of the mode. A stationary car and a receiver that has never seen a
// satellite both have a speed of zero, and a debug screen that renders them the same way
// is worse than no screen, because it would send someone looking for a fault in the wrong
// place - or declare the wiring good when nothing is connected.
void an_absent_receiver_shows_nothing_rather_than_zeroes()
{
    const auto view = ui::gps_only_view({});
    assert(view.state == ui::GpsOnlyState::no_receiver);
    assert(std::strcmp(view.speed.data(), kNoValue) == 0);
    assert(std::strcmp(view.status.data(), "NO RECEIVER") == 0);
    for (const auto& entry : view.rows) {
        assert(std::strlen(entry.label.data()) > 0);
        assert(std::strcmp(entry.value.data(), kNoValue) == 0);
    }
}

// A cold start outdoors can take minutes, and during it the receiver is working. Satellite
// count, frame rate and fix type are reported throughout, and watching the count climb is
// how a slow start is told apart from a receiver that is not talking at all.
void a_searching_receiver_reports_what_it_already_knows()
{
    auto snapshot = fixed_sample();
    snapshot.fix_valid = false;
    snapshot.fix.fix_type = domain::FixType::no_fix;
    snapshot.fix.num_satellites = 4;

    const auto view = ui::gps_only_view(snapshot);
    assert(view.state == ui::GpsOnlyState::no_fix);
    assert(std::strcmp(view.status.data(), "SEARCHING") == 0);
    assert(std::strcmp(row(view, "SATS").value.data(), "4") == 0);
    assert(std::strcmp(row(view, "FIX").value.data(), "NO FIX") == 0);
    assert(std::strcmp(row(view, "RATE").value.data(), "25.0 Hz") == 0);
    assert(std::strcmp(row(view, "GAPS").value.data(), "0") == 0);

    // Everything that needs a fix withholds itself, including the speed.
    assert(std::strcmp(view.speed.data(), kNoValue) == 0);
    assert(std::strcmp(view.speed_other.data(), "--- m/s") == 0);
    assert(shows_no_value(view, "LAT"));
    assert(shows_no_value(view, "LON"));
    assert(shows_no_value(view, "ALT"));
    assert(shows_no_value(view, "HDG"));
    assert(shows_no_value(view, "H.ACC"));
    assert(shows_no_value(view, "S.ACC"));
}

void a_fix_reports_road_speed_in_mph_and_the_receiver_unit_alongside()
{
    const auto view = ui::gps_only_view(fixed_sample());
    assert(view.state == ui::GpsOnlyState::fixed);
    assert(std::strcmp(view.status.data(), "FIX OK") == 0);
    assert(std::strcmp(view.speed.data(), "140") == 0);
    assert(std::strcmp(view.speed_other.data(), "62.6 m/s") == 0);
}

// The converse of the absent-receiver rule: once there is a fix, a genuine zero is data
// and must read as zero.
void a_stationary_car_with_a_fix_reads_zero_not_a_placeholder()
{
    auto snapshot = fixed_sample();
    snapshot.fix.speed_mps = 0.0F;
    const auto view = ui::gps_only_view(snapshot);
    assert(std::strcmp(view.speed.data(), "0") == 0);
    assert(std::strcmp(view.speed_other.data(), "0.0 m/s") == 0);
}

// Six decimal places is about 0.11 m of latitude, so the numbers visibly move when the car
// does. Fewer would sit still at walking pace, which is the speed this is checked at.
void position_is_rendered_finely_enough_to_see_movement()
{
    const auto view = ui::gps_only_view(fixed_sample());
    assert(std::strcmp(row(view, "LAT").value.data(), "52.430552") == 0);
    assert(std::strcmp(row(view, "LON").value.data(), "-1.477713") == 0);
    assert(std::strcmp(row(view, "ALT").value.data(), "118 m") == 0);
    assert(std::strcmp(row(view, "HDG").value.data(), "274") == 0);
    assert(std::strcmp(row(view, "H.ACC").value.data(), "0.8 m") == 0);
    assert(std::strcmp(row(view, "S.ACC").value.data(), "0.12 m/s") == 0);
    assert(std::strcmp(row(view, "SATS").value.data(), "14") == 0);
    assert(std::strcmp(row(view, "FIX").value.data(), "3D") == 0);
}

// Dropped fixes are the one number on this screen that the receiver does not report: it is
// counted from gaps in its own sequence numbering, and it is how a starved transport is
// caught. A zero that is never anything else would be no evidence at all.
void dropped_fixes_are_surfaced()
{
    auto snapshot = fixed_sample();
    snapshot.dropped_fixes = 37;
    snapshot.observed_rate_hz = 23.5F;
    const auto view = ui::gps_only_view(snapshot);
    assert(std::strcmp(row(view, "GAPS").value.data(), "37") == 0);
    assert(std::strcmp(row(view, "RATE").value.data(), "23.5 Hz") == 0);
}

// The three states have to be distinguishable without reading, since the screen is looked
// at from a driving position.
void each_state_has_its_own_colour_and_name()
{
    ui::GpsOnlySnapshot absent{};
    auto searching = fixed_sample();
    searching.fix_valid = false;

    const auto no_receiver = ui::gps_only_view(absent);
    const auto no_fix = ui::gps_only_view(searching);
    const auto fixed = ui::gps_only_view(fixed_sample());

    assert(no_receiver.status_rgb != no_fix.status_rgb);
    assert(no_fix.status_rgb != fixed.status_rgb);
    assert(no_receiver.status_rgb != fixed.status_rgb);

    assert(std::strcmp(ui::gps_only_state_name(ui::GpsOnlyState::no_receiver),
                       "no-receiver") == 0);
    assert(std::strcmp(ui::gps_only_state_name(ui::GpsOnlyState::no_fix), "no-fix") == 0);
    assert(std::strcmp(ui::gps_only_state_name(ui::GpsOnlyState::fixed), "fixed") == 0);
}

void every_fix_type_has_a_label()
{
    for (const auto type : {domain::FixType::no_fix, domain::FixType::dead_reckoning,
                            domain::FixType::fix_2d, domain::FixType::fix_3d,
                            domain::FixType::gnss_dead_reckoning,
                            domain::FixType::time_only}) {
        assert(std::strlen(ui::fix_type_label(type)) > 0);
    }
}

// Every field is a fixed array written through snprintf, so an implausible value has to
// truncate rather than run off the end. A receiver reporting nonsense is exactly the case
// this screen is pointed at.
void implausible_values_truncate_instead_of_overflowing()
{
    auto snapshot = fixed_sample();
    snapshot.fix.latitude_deg = -179.9999999;
    snapshot.fix.longitude_deg = 179.9999999;
    snapshot.fix.speed_mps = 1.0e6F;
    snapshot.fix.height_m = -1.0e7F;
    snapshot.fix.horizontal_accuracy_m = 1.0e9F;
    snapshot.fix.num_satellites = 65535;
    snapshot.dropped_fixes = 4294967295U;

    const auto view = ui::gps_only_view(snapshot);
    assert(view.speed[view.speed.size() - 1] == '\0');
    assert(std::strlen(view.speed.data()) < view.speed.size());
    for (const auto& entry : view.rows) {
        assert(entry.label[entry.label.size() - 1] == '\0');
        assert(entry.value[entry.value.size() - 1] == '\0');
        assert(std::strlen(entry.value.data()) < entry.value.size());
    }
}

void the_row_set_is_exactly_what_the_screen_lays_out()
{
    const auto view = ui::gps_only_view(fixed_sample());
    const char* const expected[ui::kGpsOnlyRowCount] = {
        "FIX", "SATS", "LAT", "LON", "ALT", "HDG", "H.ACC", "S.ACC", "RATE", "GAPS"};
    for (std::size_t index = 0; index < ui::kGpsOnlyRowCount; ++index) {
        assert(std::strcmp(view.rows[index].label.data(), expected[index]) == 0);
    }
}

}  // namespace

int main()
{
    an_absent_receiver_shows_nothing_rather_than_zeroes();
    a_searching_receiver_reports_what_it_already_knows();
    a_fix_reports_road_speed_in_mph_and_the_receiver_unit_alongside();
    a_stationary_car_with_a_fix_reads_zero_not_a_placeholder();
    position_is_rendered_finely_enough_to_see_movement();
    dropped_fixes_are_surfaced();
    each_state_has_its_own_colour_and_name();
    every_fix_type_has_a_label();
    implausible_values_truncate_instead_of_overflowing();
    the_row_set_is_exactly_what_the_screen_lays_out();

    std::cout << "GPS Only view model: absent, searching and fixed receivers each render "
                 "distinctly, and no unknown value renders as a zero\n";
    return 0;
}
