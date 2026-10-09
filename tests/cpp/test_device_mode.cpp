#include "track_timer/ui/device_mode.hpp"
#include "track_timer/ui/time_roller.hpp"
#include "track_timer/ui/value_picker.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

namespace {

using namespace track_timer;

settings::DeviceSettings base()
{
    settings::DeviceSettings value{};
    value.session_duration_seconds = 20;
    value.rest_duration_seconds = 20;
    return value;
}

// Mode is derived from settings that already exist, so selecting a mode must round-trip
// through them without a schema change.
void every_mode_round_trips_through_settings()
{
    for (const auto mode : {ui::DeviceMode::track_day, ui::DeviceMode::race,
                            ui::DeviceMode::g_only, ui::DeviceMode::gps_only}) {
        auto settings = base();
        ui::apply_mode(mode, settings);
        assert(ui::mode_from_settings(settings) == mode);
    }
}

void modes_map_onto_the_existing_settings_fields()
{
    auto settings = base();

    ui::apply_mode(ui::DeviceMode::track_day, settings);
    assert(settings.operating_mode == settings::OperatingMode::timer);
    assert(settings.trackday_mode_enabled);

    ui::apply_mode(ui::DeviceMode::race, settings);
    assert(settings.operating_mode == settings::OperatingMode::timer);
    assert(!settings.trackday_mode_enabled);

    ui::apply_mode(ui::DeviceMode::g_only, settings);
    assert(settings.operating_mode == settings::OperatingMode::g_meter);

    ui::apply_mode(ui::DeviceMode::gps_only, settings);
    assert(settings.operating_mode == settings::OperatingMode::gps_only);
    assert(!settings.trackday_mode_enabled);
}

// The combinations the UI never writes still have to resolve to something definite.
void the_operating_mode_wins_over_a_stale_trackday_flag()
{
    auto settings = base();
    settings.operating_mode = settings::OperatingMode::g_meter;
    settings.trackday_mode_enabled = true;
    assert(ui::mode_from_settings(settings) == ui::DeviceMode::g_only);

    settings.operating_mode = settings::OperatingMode::gps_only;
    assert(ui::mode_from_settings(settings) == ui::DeviceMode::gps_only);
}

// A debug view of the receiver and nothing else. A session timer here would invite using
// this mode to time something, which is the one thing it is not for.
void gps_only_shows_receiver_data_and_nothing_else()
{
    const auto visibility = ui::visibility_for(ui::DeviceMode::gps_only);
    assert(visibility.gnss_data);
    assert(!visibility.session_timer);
    assert(!visibility.lap_times);
    assert(!visibility.lap_delta);
    assert(!visibility.g_meter);
}

// Every mode has to be namable and labellable, or the carousel and the boot log can
// disagree with what is actually set.
void every_mode_has_a_name_a_label_and_a_summary()
{
    for (std::size_t index = 0; index < ui::kDeviceModeCount; ++index) {
        const auto mode = static_cast<ui::DeviceMode>(index);
        assert(std::strcmp(ui::device_mode_name(mode), "unknown") != 0);
        assert(std::strlen(ui::device_mode_label(mode)) > 0);
        assert(std::strlen(ui::device_mode_summary(mode)) > 0);
    }
}

// Track Day withholding live lap times is a regulatory rule, not a preference, so it is
// asserted rather than left implicit.
void track_day_shows_the_timer_only()
{
    const auto visibility = ui::visibility_for(ui::DeviceMode::track_day);
    assert(visibility.session_timer);
    assert(!visibility.lap_times);
    assert(!visibility.lap_delta);
    assert(!visibility.g_meter);
}

void race_shows_laps_delta_and_remaining()
{
    const auto visibility = ui::visibility_for(ui::DeviceMode::race);
    assert(visibility.session_timer);
    assert(visibility.lap_times);
    assert(visibility.lap_delta);
    assert(!visibility.g_meter);
}

void g_only_shows_nothing_but_the_meter()
{
    const auto visibility = ui::visibility_for(ui::DeviceMode::g_only);
    assert(visibility.g_meter);
    assert(!visibility.session_timer);
    assert(!visibility.lap_times);
    assert(!visibility.lap_delta);
    assert(!visibility.gnss_data);
}

// The point of the rework: every value is one press away, never a stepping run. The time
// fields are the exception, and deliberately so - no list of twelve can span 0 to 59:59,
// so they are edited on the roller and report no choices at all.
void every_field_offers_its_values_directly()
{
    const auto settings = base();
    for (const auto field : ui::kPickerFields) {
        const auto list = ui::choices_for(field, settings);
        assert(std::strlen(ui::picker_field_label(field)) > 0);
        if (ui::is_time_field(field)) {
            assert(list.count == 0);
            continue;
        }
        assert(list.count > 0);
        assert(list.count <= ui::kValueChoiceCapacity);
        assert(list.selected < list.count);
        assert(std::strlen(list.choices[0].text.data()) > 0);
    }
}

// The preset ladder stopped at 3:00 against a field that holds 59:59, so a circuit with a
// longer lap could not be configured at all. The roller reaches every second of the range.
void any_average_lap_is_reachable_on_the_roller()
{
    auto settings = base();
    const auto spec = ui::time_field_spec(ui::SettingsField::average_lap);
    assert(spec.maximum_seconds == 59 * 60 + 59);

    ui::TimeRoller roller{};
    roller.reset(0, spec);
    roller.step(ui::RollerColumn::minutes, 8);
    roller.step(ui::RollerColumn::seconds, 17);
    assert(roller.total_seconds() == 8 * 60 + 17);
    assert(ui::apply_time_field(ui::SettingsField::average_lap, roller.committed_seconds(),
                                settings));
    assert(settings.average_lap_seconds == 8 * 60 + 17);
}

void the_selected_index_tracks_the_current_value()
{
    auto settings = base();
    settings.day_brightness_percent = 75;
    const auto list = ui::choices_for(ui::SettingsField::day_brightness, settings);
    assert(std::strcmp(list.choices[list.selected].text.data(), "75%") == 0);
}

// A stale screen must not be able to write a value the field does not have.
void out_of_range_choices_are_refused()
{
    auto settings = base();
    assert(!ui::apply_choice(ui::SettingsField::session_duration, 99, settings));
    assert(!ui::apply_choice(ui::SettingsField::auto_dim, 2, settings));
    assert(!ui::apply_choice(ui::SettingsField::orientation, 5, settings));
    assert(settings.session_duration_seconds == 20);
}

// Dependent settings must stay consistent, matching the existing editor's rule.
void turning_off_average_lap_resets_the_dependent_field()
{
    auto settings = base();
    settings.average_lap_seconds = 90;
    settings.lower_display = settings::LowerDisplayMode::laps_remaining;

    // Rolling both columns back to 00:00 is now how the average lap is cleared.
    assert(ui::apply_time_field(ui::SettingsField::average_lap, 0, settings));
    assert(settings.average_lap_seconds == 0);
    assert(settings.lower_display == settings::LowerDisplayMode::elapsed);

    // And laps-remaining cannot be chosen again while there is nothing to count against.
    assert(!ui::apply_choice(ui::SettingsField::lower_display, 1, settings));
}

// Mode owns these, so offering them again in the field list would let the two disagree.
void mode_owned_settings_are_not_offered_as_fields()
{
    for (const auto field : ui::kPickerFields) {
        assert(field != ui::SettingsField::operating_mode);
        assert(field != ui::SettingsField::trackday_mode);
    }
}

}  // namespace

int main()
{
    every_mode_round_trips_through_settings();
    modes_map_onto_the_existing_settings_fields();
    the_operating_mode_wins_over_a_stale_trackday_flag();
    gps_only_shows_receiver_data_and_nothing_else();
    every_mode_has_a_name_a_label_and_a_summary();
    track_day_shows_the_timer_only();
    race_shows_laps_delta_and_remaining();
    g_only_shows_nothing_but_the_meter();
    every_field_offers_its_values_directly();
    any_average_lap_is_reachable_on_the_roller();
    the_selected_index_tracks_the_current_value();
    out_of_range_choices_are_refused();
    turning_off_average_lap_resets_the_dependent_field();
    mode_owned_settings_are_not_offered_as_fields();

    std::cout << "Mode derivation, per-mode visibility rules, and one-press direct value "
                 "selection passed\n";
    return 0;
}
