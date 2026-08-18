#include "track_timer/ui/device_mode.hpp"
#include "track_timer/ui/value_picker.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

namespace {

using namespace track_timer;

settings::DeviceSettings base()
{
    settings::DeviceSettings value{};
    value.session_duration_minutes = 20;
    value.rest_duration_minutes = 20;
    return value;
}

// Mode is derived from settings that already exist, so selecting a mode must round-trip
// through them without a schema change.
void every_mode_round_trips_through_settings()
{
    for (const auto mode : {ui::DeviceMode::track_day, ui::DeviceMode::race,
                            ui::DeviceMode::g_only}) {
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
}

// The combination the UI never writes still has to resolve to something definite.
void g_meter_wins_over_a_stale_trackday_flag()
{
    auto settings = base();
    settings.operating_mode = settings::OperatingMode::g_meter;
    settings.trackday_mode_enabled = true;
    assert(ui::mode_from_settings(settings) == ui::DeviceMode::g_only);
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
}

// The point of the rework: every value is one press away, never a stepping run.
void every_field_offers_its_values_directly()
{
    const auto settings = base();
    for (const auto field : ui::kPickerFields) {
        const auto list = ui::choices_for(field, settings);
        assert(list.count > 0);
        assert(list.count <= ui::kValueChoiceCapacity);
        assert(list.selected < list.count);
        assert(std::strlen(list.choices[0].text.data()) > 0);
        assert(std::strlen(ui::picker_field_label(field)) > 0);
    }
}

// The old editor needed up to 150 presses to reach a 2:30 average lap by stepping.
void average_lap_is_reachable_in_one_press()
{
    auto settings = base();
    const auto list = ui::choices_for(ui::SettingsField::average_lap, settings);
    std::size_t target = list.count;
    for (std::size_t index = 0; index < list.count; ++index) {
        if (std::strcmp(list.choices[index].text.data(), "2:30") == 0) {
            target = index;
        }
    }
    assert(target < list.count);
    assert(ui::apply_choice(ui::SettingsField::average_lap, target, settings));
    assert(settings.average_lap_seconds == 150);
}

void the_selected_index_tracks_the_current_value()
{
    auto settings = base();
    settings.session_duration_minutes = 40;
    const auto list = ui::choices_for(ui::SettingsField::session_duration, settings);
    assert(std::strcmp(list.choices[list.selected].text.data(), "40 MIN") == 0);
}

// A stale screen must not be able to write a value the field does not have.
void out_of_range_choices_are_refused()
{
    auto settings = base();
    assert(!ui::apply_choice(ui::SettingsField::session_duration, 99, settings));
    assert(!ui::apply_choice(ui::SettingsField::auto_dim, 2, settings));
    assert(!ui::apply_choice(ui::SettingsField::orientation, 5, settings));
    assert(settings.session_duration_minutes == 20);
}

// Dependent settings must stay consistent, matching the existing editor's rule.
void turning_off_average_lap_resets_the_dependent_field()
{
    auto settings = base();
    settings.average_lap_seconds = 90;
    settings.lower_display = settings::LowerDisplayMode::laps_remaining;

    const auto list = ui::choices_for(ui::SettingsField::average_lap, settings);
    assert(std::strcmp(list.choices[0].text.data(), "OFF") == 0);
    assert(ui::apply_choice(ui::SettingsField::average_lap, 0, settings));
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
    g_meter_wins_over_a_stale_trackday_flag();
    track_day_shows_the_timer_only();
    race_shows_laps_delta_and_remaining();
    g_only_shows_nothing_but_the_meter();
    every_field_offers_its_values_directly();
    average_lap_is_reachable_in_one_press();
    the_selected_index_tracks_the_current_value();
    out_of_range_choices_are_refused();
    turning_off_average_lap_resets_the_dependent_field();
    mode_owned_settings_are_not_offered_as_fields();

    std::cout << "Mode derivation, per-mode visibility rules, and one-press direct value "
                 "selection passed\n";
    return 0;
}
